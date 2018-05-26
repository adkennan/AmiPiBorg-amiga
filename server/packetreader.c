
#include "packetreader.h"
#include "buffer.h"
#include "stats.h"
#include "memory.h"
#include "protocol.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct PacketReader
{
    MemoryPool          pr_MemPool;
    ObjectPool          pr_ObjPool;
    struct InBuffer	   *pr_InBuf;
    struct MinList      pr_Queue;
    UWORD               pr_QueueSize;
};

struct InBuffer
{
    struct PacketReader *ib_Reader;
    struct Buffer       *ib_Buf;
    struct InBuffer	    *ib_Next;
    UWORD                ib_Offset;
    UWORD                ib_PacketCount;
    BOOL                 ib_CanRelease;
};

#define PR_QUEUE(pr) ((struct List *)&pr->pr_Queue)

#define BUF_BYTES(ib) (ib->ib_Buf->b_Offset)


// Packet Reader

PacketReader APB_CreatePacketReader(MemoryPool memPool, ObjectPool objPool)
{
    struct PacketReader *pr;

    if( ! (pr = APB_AllocMem(memPool, sizeof(struct PacketReader) ) ) ) {
        return NULL;
    }

    pr->pr_MemPool = memPool;
    pr->pr_ObjPool = objPool;
    pr->pr_InBuf = NULL;
    pr->pr_QueueSize = 0;
    NewList(PR_QUEUE(pr));

    if( ! APB_TypeRegistered(objPool, OT_IN_BUFFER) ) {    
        APB_RegisterObjectType(objPool, OT_IN_BUFFER, sizeof(struct InBuffer), MIN_BUFFERS, MAX_BUFFERS);
    }

    if( ! APB_TypeRegistered(objPool, OT_IN_PACKET) ) {
        APB_RegisterObjectType(objPool, OT_IN_PACKET, sizeof(struct InPacket), MIN_BUFFERS * 10, MAX_BUFFERS * 10);
    }

    return pr;
}

VOID APB_ReleaseInBuffer(struct InBuffer *ib)
{
    APB_ReleaseBuffer(ib->ib_Buf);

    APB_FreeObject(ib->ib_Reader->pr_ObjPool, OT_IN_BUFFER, ib);

    APB_IncrementStat(ST_IB_ALLOCATED, -1);
}

VOID APB_DestroyPacketReader(PacketReader packetReader)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;
	struct InBuffer *ib = pr->pr_InBuf;
	while( ib != NULL ) {
		ib = ib->ib_Next;
	}

    pr->pr_InBuf = NULL;

    while(pr->pr_QueueSize > 0 ) {
        APB_ReleaseInPacket(APB_DequeueInPacket(packetReader));
    }

    APB_FreeMem(pr->pr_MemPool, pr, sizeof(struct PacketReader));
}

BOOL APB_AppendBuffer(struct PacketReader *pr, struct Buffer *buf)
{
    struct InBuffer *ib, *ib2;
    BOOL createNew = TRUE;

    ib2 = pr->pr_InBuf;
    while( ib2 != NULL ) {
        if( ib2->ib_Buf == buf ) {
            createNew = FALSE;
            break;
        }

        ib2 = ib2->ib_Next;
    }            
    
    if( createNew ) {

        if( ! (ib = (struct InBuffer *)APB_AllocObject(pr->pr_ObjPool, OT_IN_BUFFER) ) ) {

            APB_IncrementStat(ST_IB_ALLOC_FAILURES, 1);
            return FALSE;

        } else {

            APB_IncrementStat(ST_IB_ALLOCATED, 1);
            ib->ib_Next = NULL;
            ib->ib_Reader = pr;
            ib->ib_Buf = buf;
            ib->ib_Offset = 0;
            ib->ib_PacketCount = 0;
            ib->ib_CanRelease = FALSE;
        }

        if( pr->pr_InBuf == NULL ) {
            pr->pr_InBuf = ib;
    
        } else {
            ib2 = pr->pr_InBuf;    
            ib2->ib_CanRelease = TRUE;
            while(ib2->ib_Next != NULL) {
                ib2 = ib2->ib_Next;
	            ib2->ib_CanRelease = TRUE;
            }
            ib2->ib_Next = ib;
        }              
    }

    return TRUE;
}

VOID APB_ProcessBuffer(PacketReader packetReader, struct Buffer *buf)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    struct InBuffer  *ib1, *ib2;
    struct InPacket  *ip;
	struct Packet	  p;
	UWORD bc, dataLen1, dataLen2, consumed, dl, pc, oddSize;
	BYTE *data1, *data2;
	ULONG sum;

	if( ! APB_AppendBuffer(pr, buf) ) {
        return;
    }
    
    ib1 = pr->pr_InBuf;

	pc = 0;

    while( ib1 != NULL && ib1->ib_Offset <= BUF_BYTES(ib1) ) {
		
		bc = BUF_BYTES(ib1) - ib1->ib_Offset;

		ib2 = NULL;

		dataLen1 = 0;
		data1 = NULL;

		dataLen2 = 0;
		data2 = NULL;

		oddSize = 0;

		if( bc < sizeof(struct Packet) ) {

			if( BUF_BYTES(ib1) < BUFFER_SIZE ) {
				break;
			}
		
			ib2 = ib1->ib_Next;
			if( ib2 == NULL ) {
				break;
			}

			consumed = sizeof(struct Packet) - bc;

			if( BUF_BYTES(ib2) < consumed ) {
				break;
			}

			CopyMem(APB_PointerAdd(ib1->ib_Buf->b_Data, ib1->ib_Offset), &p, bc);
			CopyMem(ib2->ib_Buf->b_Data, APB_PointerAdd(&p, bc), consumed);
		
		} else {
			consumed = sizeof(struct Packet);
			CopyMem(APB_PointerAdd(ib1->ib_Buf->b_Data, ib1->ib_Offset), &p, consumed);
		}

        if( p.pac_Id != PACKET_ID ) {
			printf("%d: Invalid packet id %x\n", pc, p.pac_Id);
			goto badData;
        }

		if( p.pac_Length > BUFFER_SIZE ) {
			printf("%d: Invalid packet length: %d\n", pc, p.pac_Length);
			goto badData;
		}

		if( p.pac_Length > 0 ) {

			if( p.pac_Flags & PF_PAD_BYTE ) {
				oddSize = 1;
			}

			dl = p.pac_Length + oddSize;

			if( ib2 == NULL ) {

				if( bc == sizeof(struct Packet) && BUF_BYTES(ib1) == BUFFER_SIZE ) {
					ib2 = ib1->ib_Next;
					if( ib2 == NULL ) {
						break;
					}

					if( BUF_BYTES(ib2) < dl ) {
						break;
					}

					data1 = ib2->ib_Buf->b_Data;
					dataLen1 = p.pac_Length;
					consumed = p.pac_Length;

			        APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);

				} else if( bc - sizeof(struct Packet) < dl ) {

					if( BUF_BYTES(ib1) < BUFFER_SIZE ) {
						break;
					}

					ib2 = ib1->ib_Next;
					if( ib2 == NULL ) {
						break;
					}

					data1 = APB_PointerAdd(ib1->ib_Buf->b_Data, ib1->ib_Offset + sizeof(struct Packet));
					data2 = ib2->ib_Buf->b_Data;
					dataLen1 = bc - sizeof(struct Packet);
					dataLen2 = p.pac_Length - dataLen1;

					consumed = dataLen2;

					if( BUF_BYTES(ib2) < dataLen2 ) {
						break;
					}	

			        APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);

				} else {

					data1 = APB_PointerAdd(ib1->ib_Buf->b_Data, ib1->ib_Offset + sizeof(struct Packet));
					dataLen1 = p.pac_Length;
					consumed += dataLen1;
				}

			} else {

				if( BUF_BYTES(ib2) < dl + consumed ) {
					break;
				}

				data1 = APB_PointerAdd(ib2->ib_Buf->b_Data, sizeof(struct Packet) - bc);
				dataLen1 = p.pac_Length;
				consumed += dataLen1;

		        APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
			}
		}

		sum = APB_AddToChecksum(0, (UWORD *)&p, sizeof(struct Packet));
		if( dataLen1 > 0 ) {	
	
			if( dataLen2 > 0 ) {

				sum = APB_AddToChecksum(sum, (UWORD *)data1, dataLen1);
				sum = APB_AddToChecksum(sum, (UWORD *)data2, dataLen2 + oddSize);
			} else {
				sum = APB_AddToChecksum(sum, (UWORD *)data1, dataLen1 + oddSize);
			}
		}

		if( APB_CompleteChecksum(sum) != 0xFFFF ) {
			printf("%d: Invalid checksum, %x\n", pc, APB_CompleteChecksum(sum));
			goto badData;
		}

        if( ! (ip = APB_AllocObject(pr->pr_ObjPool, OT_IN_PACKET) ) ) {
            APB_IncrementStat(ST_IP_ALLOC_FAILURES, 1);
            break;
        }

        APB_IncrementStat(ST_IP_ALLOCATED, 1);

        ip->ip_InBuf = ib1;
        ip->ip_Type = p.pac_Type;
        ip->ip_Flags = p.pac_Flags;
        ip->ip_ConnId = p.pac_ConnId;
        ip->ip_PackId = p.pac_PackId;
        ip->ip_Data1Length = dataLen1;
        ip->ip_Data1 = data1;
        ip->ip_Data2Length = dataLen2;
        ip->ip_Data2 = data2;
        ip->ip_Offset = 0;

        AddTail(PR_QUEUE(pr), (struct Node *)ip);
        pr->pr_QueueSize++;

        ib1->ib_PacketCount++;
        if( ib2 ) {
            ib2->ib_PacketCount++;
	        pr->pr_InBuf = ib2;

			ib1 = ib2;
        } else {          
	        pr->pr_InBuf = ib1;
		}

        APB_IncrementStat(ST_Q_INCOMING_SIZE, 1);

		consumed += oddSize;

		ib1->ib_Offset += consumed;

		APB_IncrementStat(ST_BYTES_RECEIVED, consumed);            

        if( ib1->ib_Offset == BUFFER_SIZE ) {
            ib1 = ib1->ib_Next;
        }

		pc++;

		continue;

badData:

		if( ib2 ) {	
			ib1 = ib2;
		}

		if( ib1->ib_Offset < BUF_BYTES(ib1) ) {
			ib1->ib_Offset += 2;
		} else if( BUF_BYTES(ib1) == BUFFER_SIZE && ib1->ib_Next ) {		
			pr->pr_InBuf = ib1->ib_Next;		
		}

		APB_IncrementStat(ST_IP_INVALID_DATA, 1);


		if( BUF_BYTES(ib1) > BUFFER_SIZE ) {
			printf("WTF!\n");
			break;
		}

	}
}


VOID APB_ReleaseInPacket(struct InPacket *ip)
{
    struct InBuffer *ib = ip->ip_InBuf;

    APB_FreeObject(ib->ib_Reader->pr_ObjPool, OT_IN_PACKET, ip);

    ib->ib_PacketCount--;
    if( ib->ib_PacketCount == 0 && ib->ib_CanRelease ) {
        APB_ReleaseInBuffer(ib);
    }
    
    if( ib = ib->ib_Next ) {
        ib->ib_PacketCount--;
        if( ib->ib_PacketCount == 0 && ib->ib_CanRelease ) {
            APB_ReleaseInBuffer(ib);
		}
    }

    APB_IncrementStat(ST_IP_ALLOCATED, -1);
}

UWORD APB_ReaderQueueSize(PacketReader pr)
{
    return ((struct PacketReader *)pr)->pr_QueueSize;
}

struct InPacket *APB_DequeueInPacket(PacketReader packetReader)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    if( pr->pr_QueueSize == 0 ) {

        return NULL;
    }

    pr->pr_QueueSize--;
    APB_IncrementStat(ST_Q_INCOMING_SIZE, -1);
    APB_IncrementStat(ST_PAC_RECEIVED, 1);    
        
    return (struct InPacket *)RemHead(PR_QUEUE(pr));
}

