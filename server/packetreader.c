
#include "packetreader.h"
#include "buffer.h"
#include "stats.h"
#include "memory.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct BufferRef
{
    struct PacketReader *br_Reader;
    struct Buffer       *br_Buf;
    struct BufferRef    *br_Next;
    UWORD                br_Offset;
    UWORD                br_PacketCount;
    BOOL                 br_CanRelease;
};

struct PacketReader
{
    MemoryPool          pr_MemPool;
    ObjectPool          pr_ObjPool;
    struct BufferRef   *pr_BufRef;
    struct MinList      pr_Queue;
    UWORD               pr_QueueSize;
};

#define PR_QUEUE(pr) ((struct List *)&pr->pr_Queue)

#define PACK_BR(packRef) ((struct BufferRef *)packRef->pr_BufRef)

#define TO_PACKET(br, offset) (((struct Packet *)APB_PointerAdd(br->br_Buf->b_Data, offset)))
#define TO_DATA1(pa) (APB_PointerAdd(pa, sizeof(struct Packet)))

#define BUF_BYTES(br) (br->br_Buf->b_Offset)


// Packet Reader

PacketReader APB_CreatePacketReader(MemoryPool memPool, ObjectPool objPool)
{
    struct PacketReader *pr;

    if( ! (pr = APB_AllocMem(memPool, sizeof(struct PacketReader) ) ) ) {
        return NULL;
    }

    pr->pr_MemPool = memPool;
    pr->pr_ObjPool = objPool;
    pr->pr_BufRef = NULL;
    pr->pr_QueueSize = 0;
    NewList(PR_QUEUE(pr));

    if( ! APB_TypeRegistered(objPool, OT_BUFFER_REF) ) {    
        APB_RegisterObjectType(objPool, OT_BUFFER_REF, sizeof(struct BufferRef), MIN_BUFFERS, MAX_BUFFERS);
    }

    if( ! APB_TypeRegistered(objPool, OT_PACKET_REF) ) {
        APB_RegisterObjectType(objPool, OT_PACKET_REF, sizeof(struct PacketRef), MIN_BUFFERS * 10, MAX_BUFFERS * 10);
    }

    return pr;
}

VOID APB_ReleaseBufferRef(struct BufferRef *br)
{
    APB_ReleaseBuffer(br->br_Buf);

    APB_FreeObject(br->br_Reader->pr_ObjPool, OT_BUFFER_REF, br);

    APB_IncrementStat(ST_BR_ALLOCATED, -1);
}

VOID APB_DestroyPacketReader(PacketReader packetReader)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    pr->pr_BufRef = NULL;

    while(pr->pr_QueueSize > 0 ) {
        APB_ReleasePacketRef(APB_DequeuePacketRef(packetReader));
    }

    APB_FreeMem(pr->pr_MemPool, pr, sizeof(struct PacketReader));
}

BOOL APB_AppendBuffer(struct PacketReader *pr, struct Buffer *buf)
{
    struct BufferRef *br, *br2;
    BOOL createNew = TRUE;

    br2 = pr->pr_BufRef;
    while( br2 != NULL ) {
        if( br2->br_Buf == buf ) {
            createNew = FALSE;
            break;
        }

        br2 = br2->br_Next;
    }            
    
    if( createNew ) {

        if( ! (br = (struct BufferRef *)APB_AllocObject(pr->pr_ObjPool, OT_BUFFER_REF) ) ) {

            APB_IncrementStat(ST_BR_ALLOC_FAILURES, 1);
            return FALSE;

        } else {

            //printf("Allocated a new buf ref.\n");

            APB_IncrementStat(ST_BR_ALLOCATED, 1);
            br->br_Next = NULL;
            br->br_Reader = pr;
            br->br_Buf = buf;
            br->br_Offset = 0;
            br->br_PacketCount = 0;
            br->br_CanRelease = FALSE;
        }

        if( pr->pr_BufRef == NULL ) {
            pr->pr_BufRef = br;
    
        } else {
            br2 = pr->pr_BufRef;    
            while(br2->br_Next != NULL) {
                br2 = br2->br_Next;
            }
            br2->br_CanRelease = TRUE;
            br2->br_Next = br;
        }              
    }

    return TRUE;
}

VOID APB_ProcessBuffer(PacketReader packetReader, struct Buffer *buf)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    struct BufferRef *br, *headBr;
    struct PacketRef *p;
    struct Packet    *pa;
    ULONG             id;
    WORD              offset, bytesLeft;
    UWORD             packId, connId, packLen1, packLen2;
    UBYTE             packType, packFlags;
    UBYTE            *packData1, *packData2;

    if( ! APB_AppendBuffer(pr, buf) ) {
        return;
    }
    
    br = pr->pr_BufRef;

    offset = br->br_Offset;

        while( br != NULL && offset <= BUF_BYTES(br) ) {

			packLen2 = 0;
			packData2 = 0;
            headBr = br;

            pa = TO_PACKET(br, offset);    

            bytesLeft = BUF_BYTES(br) - offset;
            if( bytesLeft == 0 ) {
                br = br->br_Next;
                continue;
            }
                
            if( bytesLeft <= sizeof(struct Packet) ) {

                if( br->br_Next == NULL ) {
                    // Not enough space left for a packet, and no more buffers.
                    return;
                }

                printf("Packet header split %d bytes left.\n", bytesLeft);

                switch(bytesLeft) {

                    case 2:
                        id                  = (*(UWORD *)pa) << 16;

                        br = br->br_Next;
						if( BUF_BYTES(br) < sizeof(struct Packet) - bytesLeft ) {
							return;
						}
                        offset = -bytesLeft;

                        pa = TO_PACKET(br, offset);    

                        id                 |= *(((UWORD *)pa) + 1);

                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;
                        connId              = pa->pac_ConnId;
                        packId              = pa->pac_PackId;
                        packLen1            = pa->pac_Length;           
                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;

                    case 4:
                        id                  = pa->pac_Id;

                        br = br->br_Next;
                        if( BUF_BYTES(br) < sizeof(struct Packet) - bytesLeft ) {
							return;
						}			
						offset = -bytesLeft;

                        pa = TO_PACKET(br, offset);    

                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;
                        connId              = pa->pac_ConnId;
                        packId              = pa->pac_PackId;
                        packLen1            = pa->pac_Length;           
                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;

                    case 6:
                        id                  = pa->pac_Id;
                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;

                        br = br->br_Next;
						if( BUF_BYTES(br) < sizeof(struct Packet) - bytesLeft ) {
							return;
						}		
                        offset = -bytesLeft;

                        pa = TO_PACKET(br, offset);    

                        connId              = pa->pac_ConnId;
                        packId              = pa->pac_PackId;
                        packLen1            = pa->pac_Length;           
                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;

    
                    case 8:
                        id                  = pa->pac_Id;
                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;
                        connId              = pa->pac_ConnId;

                        br = br->br_Next;
						if( BUF_BYTES(br) < sizeof(struct Packet) - bytesLeft ) {
							return;
						}

                        offset = -bytesLeft;

                        pa = TO_PACKET(br, offset);    
    
                        packId              = pa->pac_PackId;
                        packLen1            = pa->pac_Length;
                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;

                    case 10:
                    case 12:
                        id                  = pa->pac_Id;
                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;
                        connId              = pa->pac_ConnId;
                        packId              = pa->pac_PackId;

                        br = br->br_Next;
						if( BUF_BYTES(br) < sizeof(struct Packet) - bytesLeft ) {
							return;
						}

                        offset = -bytesLeft;

                        pa = TO_PACKET(br, offset);    
                
                        packLen1            = pa->pac_Length;
                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;

                    case 14:
                        id                  = pa->pac_Id;
                        packType            = pa->pac_Type;
                        packFlags           = pa->pac_Flags;
                        connId              = pa->pac_ConnId;
                        packId              = pa->pac_PackId;
                        packLen1            = pa->pac_Length;

                        br = br->br_Next;

                        offset = -bytesLeft;
                
                        pa = TO_PACKET(br, offset);    

                        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(br) + offset < packLen1 ) {
							return;
						}

                        break;                                        

					default:
						
						printf("%d bytes. WTF?\n");
                        br = br->br_Next;
                        offset = 0;
                        continue;
                }

                APB_IncrementStat(ST_PR_SPLIT_COUNT, 1);

            } else {
                id                  = pa->pac_Id;
                packType            = pa->pac_Type;
                packFlags           = pa->pac_Flags;
                connId              = pa->pac_ConnId;
                packId              = pa->pac_PackId;
                packLen1            = pa->pac_Length;
                packData1           = TO_DATA1(pa);    

	            if( bytesLeft - sizeof(struct Packet) < packLen1 ) {

            	    if( br->br_Next == NULL ) {
	                    // Not enough space left for a packet, and no more buffers.
        	            return;
    	            }

					printf("Packet data split %d bytes left, %d expected.\n", bytesLeft - sizeof(struct Packet), packLen1);

                	packLen2 = (sizeof(struct Packet) + packLen1) - bytesLeft;
            	    packLen1 = bytesLeft - sizeof(struct Packet);
                
        	        br = br->br_Next;
    	            offset = -bytesLeft;

	                packData2 = br->br_Buf->b_Data;
					if( packLen1 + packLen2 > BUFFER_SIZE ) {
						printf("Invalid packet size at %d: %d\n", offset, packLen1 + packLen2);
						if( br->br_Offset < br->br_Buf->b_Offset ) {
		                    br->br_Offset += 2;
        		            continue;                
                		}
					}

					if( BUF_BYTES(br) < packLen2 ) {
						return;
					}

	                APB_IncrementStat(ST_PR_SPLIT_COUNT, 1);

            	}
            }

            if( id != PACKET_ID ) {
                printf("Invalid packet at %d: 0x%x\n", offset, id);
                if( br->br_Offset < br->br_Buf->b_Offset ) {
                    br->br_Offset += 2;
                    continue;                
                }
                break;
            }

			if( packLen1 + packLen2 > BUFFER_SIZE ) {
				printf("Invalid packet size at %d: %d\n", offset, packLen1 + packLen2);
				if( br->br_Offset < br->br_Buf->b_Offset ) {
                    br->br_Offset += 2;
                    continue;                
                }
			}

            if( ! (p = APB_AllocObject(pr->pr_ObjPool, OT_PACKET_REF) ) ) {
                APB_IncrementStat(ST_PR_ALLOC_FAILURES, 1);
                break;
            }

            APB_IncrementStat(ST_PR_ALLOCATED, 1);

            p->pr_BufRef = headBr;
            p->pr_Type = packType;
            p->pr_Flags = packFlags;
            p->pr_ConnId = connId;
            p->pr_PackId = packId;
            p->pr_Data1Length = packLen1;
            p->pr_Data1 = packData1;
            p->pr_Data2Length = packLen2;
            p->pr_Data2 = packData2;
            p->pr_Offset = 0;

            headBr->br_PacketCount++;
            if( br != headBr ) {
                br->br_PacketCount++;
            }
           
            AddTail(PR_QUEUE(pr), (struct Node *)p);
            pr->pr_QueueSize++;
            pr->pr_BufRef = br;

            APB_IncrementStat(ST_Q_INCOMING_SIZE, 1);

            offset += sizeof(struct Packet) + packLen1 + packLen2;

            if( packFlags & PF_PAD_BYTE ) {
                offset++;
            }

			if( packLen2 > 0 ) {
				printf("offset = %d, packLen1 = %d, packLen2 = %d\n", offset, packLen1, packLen2);
				printf("packData1 = %ld, packData2 = %ld\n", packData1, packData2);
			}

			APB_IncrementStat(ST_BYTES_RECEIVED, offset - br->br_Offset);            

            br->br_Offset = offset;

            if( br->br_Offset == BUF_BYTES(br) ) {
                br = br->br_Next;
                offset = 0;
            }
        }
}

VOID APB_ReleasePacketRef(struct PacketRef *p)
{
    struct BufferRef *br = p->pr_BufRef;

    APB_FreeObject(br->br_Reader->pr_ObjPool, OT_PACKET_REF, p);

    br->br_PacketCount--;
    if( br->br_PacketCount == 0 && br->br_CanRelease ) {
        APB_ReleaseBufferRef(br);
    }
    
    if( br = br->br_Next ) {
        br->br_PacketCount--;
        if( br->br_PacketCount == 0 && br->br_CanRelease ) {
            APB_ReleaseBufferRef(br);
		}
    }

    APB_IncrementStat(ST_PR_ALLOCATED, -1);
}

UWORD APB_ReaderQueueSize(PacketReader pr)
{
    return ((struct PacketReader *)pr)->pr_QueueSize;
}

struct PacketRef *APB_DequeuePacketRef(PacketReader packetReader)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    if( pr->pr_QueueSize == 0 ) {

        return NULL;
    }

    pr->pr_QueueSize--;
    APB_IncrementStat(ST_Q_INCOMING_SIZE, -1);
    APB_IncrementStat(ST_PAC_RECEIVED, 1);    
        
    return (struct PacketRef *)RemHead(PR_QUEUE(pr));
}

