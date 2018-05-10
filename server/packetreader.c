
#include "packetreader.h"
#include "buffer.h"
#include "stats.h"
#include "memory.h"
#include "protocol.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct InBuffer
{
    struct PacketReader *ib_Reader;
    struct Buffer       *ib_Buf;
    struct InBuffer	    *ib_Next;
    UWORD                ib_Offset;
    UWORD                ib_PacketCount;
    BOOL                 ib_CanRelease;
};

struct PacketReader
{
    MemoryPool          pr_MemPool;
    ObjectPool          pr_ObjPool;
    struct InBuffer	   *pr_InBuf;
    struct MinList      pr_Queue;
    UWORD               pr_QueueSize;
};

#define PR_QUEUE(pr) ((struct List *)&pr->pr_Queue)

#define PACK_BR(ip) ((struct InBuffer *)ip->ip_InBuf)

#define TO_PACKET(ib, offset) (((struct Packet *)APB_PointerAdd(ib->ib_Buf->b_Data, offset)))
#define TO_DATA1(pa) (APB_PointerAdd(pa, sizeof(struct Packet)))

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

			printf("New Buffer\n");

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
            while(ib2->ib_Next != NULL) {
                ib2 = ib2->ib_Next;
            }
            ib2->ib_CanRelease = TRUE;
            ib2->ib_Next = ib;
        }              
    }

    return TRUE;
}

VOID APB_ProcessBuffer(PacketReader packetReader, struct Buffer *buf)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    struct InBuffer  *ib, *headIb;
    struct InPacket  *ip;
	struct Packet	  p;
	UWORD *src, *dst;
	UWORD bc, dataLen1, dataLen2;
	BYTE *data1, *data2;
	ULONG sum;

	if( ! APB_AppendBuffer(pr, buf) ) {
        return;
    }
    
    ib = pr->pr_InBuf;

    while( ib != NULL && ib->ib_Offset <= BUF_BYTES(ib) ) {

		headIb = ib;

		src = (UWORD *)APB_PointerAdd(ib->ib_Buf->b_Data, ib->ib_Offset);
		dst = (UWORD *)&p;

		dataLen2 = 0;
		data1 = NULL;
		data2 = NULL;

		for( bc = 0; bc < sizeof(struct Packet); bc += 2 ) {
			if( ib->ib_Offset + bc > BUF_BYTES(ib) ) {
				if( BUF_BYTES(ib) < BUFFER_SIZE ) {	
					printf("Buffer not full %d\n", bc);
					return;
				}
				ib = ib->ib_Next;
				if( ib == NULL ) {
					printf("No next buffer %d\n", bc);
					return;
				}
				if( BUF_BYTES(ib) < sizeof(struct Packet) - bc )  {
					printf("Read more bytes\n");
					return;
				}

				printf("Next buffer!\n");
				src = (UWORD *)APB_PointerAdd(ib->ib_Buf->b_Data, ib->ib_Offset);
			}
			*dst = *src;
			dst++;
			src++;
		}

		if( bc < sizeof(struct Packet) ) {
			break;
		}

        if( p.pac_Id != PACKET_ID ) {
			printf("Invalid packet id %x\n", p.pac_Id);
			goto badData;
        }

		if( p.pac_Length > BUFFER_SIZE ) {
			printf("Invalid packet length: %d\n", p.pac_Length);
			goto badData;
		}

		if( p.pac_Length > 0 ) {		
	
			data1 = (BYTE *)src;
			if( ib->ib_Offset + p.pac_Length <= BUF_BYTES(ib) ) {

				printf("Single data %lx\n", data1);

				dataLen1 = p.pac_Length;
				dataLen2 = 0;

			} else {

				if( BUF_BYTES(ib) < BUFFER_SIZE ) {
					return;
				}

				dataLen1 = BUFFER_SIZE - ib->ib_Offset - sizeof(struct Packet);
				dataLen2 = p.pac_Length - dataLen2;

				ib = ib->ib_Next;
				if( ib == NULL ) {
					return;
				}

				if( BUF_BYTES(ib) < dataLen2 ) {
					return;
				}

				data2 = ib->ib_Buf->b_Data;
			}				
		} else {
			dataLen1 = 0;
			dataLen2 = 0;
		}

		sum = APB_AddToChecksum(0, (UWORD *)&p, sizeof(struct Packet));
		if( dataLen1 > 0 ) {

			printf("Data 1, %d bytes: ", dataLen1);
			for( bc = 0; bc < dataLen1; bc++ ) {
				printf(" %c", *(((UBYTE *)src) + bc));
			}
			printf("\n");

			sum = APB_AddToChecksum(sum, (UWORD *)data1, dataLen1 + (dataLen1 & 1));
			if( dataLen2 > 0 ) {
				sum = APB_AddToChecksum(sum, (UWORD *)data2, dataLen2 + (dataLen2 & 1));
			}
		}

		if( APB_CompleteChecksum(sum) != 0xFFFF ) {
			printf("Invalid checksum, %d\n", APB_CompleteChecksum(sum));
			goto badData;
		}

        if( ! (ip = APB_AllocObject(pr->pr_ObjPool, OT_IN_PACKET) ) ) {
            APB_IncrementStat(ST_IP_ALLOC_FAILURES, 1);
            break;
        }

        APB_IncrementStat(ST_IP_ALLOCATED, 1);

        ip->ip_InBuf = headIb;
        ip->ip_Type = p.pac_Type;
        ip->ip_Flags = p.pac_Flags;
        ip->ip_ConnId = p.pac_ConnId;
        ip->ip_PackId = p.pac_PackId;
        ip->ip_Data1Length = dataLen1;
        ip->ip_Data1 = data1;
        ip->ip_Data2Length = dataLen2;
        ip->ip_Data2 = data2;
        ip->ip_Offset = 0;

        headIb->ib_PacketCount++;
        if( ib != headIb ) {
            ib->ib_PacketCount++;
        }
           
        AddTail(PR_QUEUE(pr), (struct Node *)ip);
        pr->pr_QueueSize++;
        pr->pr_InBuf = ib;

        APB_IncrementStat(ST_Q_INCOMING_SIZE, 1);

		if( dataLen2 > 0 ) {
			ib->ib_Offset += dataLen2 + (dataLen2 & 1);
		} else {
			ib->ib_Offset += sizeof(struct Packet) + dataLen1 + (dataLen1 & 1);
		}

        if( ib->ib_Offset == BUFFER_SIZE ) {
            ib = ib->ib_Next;
        }
	}

	return;
badData:
	if( ! headIb ) {
		return;
	}

	if( headIb->ib_Offset < BUF_BYTES(headIb) ) {
		headIb->ib_Offset += 2;
	} else {
		
	}

	APB_IncrementStat(ST_IP_INVALID_DATA, 1);
}

VOID APB_ProcessBufferOld(PacketReader packetReader, struct Buffer *buf)
{
    struct PacketReader *pr = (struct PacketReader *)packetReader;

    struct InBuffer  *ib, *headIb;
    struct InPacket  *ip;
    struct Packet    *pa;
    ULONG             id;
    WORD              offset, bytesLeft;
    UWORD             typeAndFlags, packId, connId, checksum, dataLen, packLen1, packLen2; 
    UBYTE             packType, packFlags;
    UBYTE            *packData1, *packData2;
	ULONG			  sum;

    if( ! APB_AppendBuffer(pr, buf) ) {
        return;
    }
    
    ib = pr->pr_InBuf;

    while( ib != NULL && ib->ib_Offset <= BUF_BYTES(ib) ) {

	    offset = ib->ib_Offset;

		packLen2 = 0;
		packData1 = NULL;
		packData2 = NULL;
	
        headIb = ib;

        pa = TO_PACKET(ib, offset);    

        bytesLeft = BUF_BYTES(ib) - offset;

		if( bytesLeft >= 4 && pa->pac_Id != PACKET_ID ) {
//			printf("Skipping 2 bytes\n");
			ib->ib_Offset += 2;
			continue;
		}

        if( bytesLeft <= sizeof(struct Packet) ) {

			if( bytesLeft < sizeof(struct Packet) ) {
				if( BUF_BYTES(ib) < BUFFER_SIZE ) {
					// Wait til the buffer is full.
					return;
				}

	            if( ib->ib_Next == NULL ) {
    	            // Not enough space left for a packet, and no more buffers.
        	        return;
            	}
			}
		
            switch(bytesLeft) {

				case 0:
					if( offset == BUFFER_SIZE ) {
						ib = ib->ib_Next;
						continue;
					}
					return;

                case 2:
                    id                  = (*(UWORD *)pa) << 16;

                    ib = ib->ib_Next;
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
					if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
					}
                    offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    

                    id |= *(((UWORD *)pa) + 1);

                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;           
					
					if( packLen1 > 0 ) {
	                    packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;

                case 4:
                    id                  = pa->pac_Id;

                    ib = ib->ib_Next;
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
                    if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
	   				}			
					offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    

                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;           
                    
					if( packLen1 > 0 ) {
						packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
	    	        	}
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;

                case 6:
                    id                  = pa->pac_Id;
                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;

                    ib = ib->ib_Next;	
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
					if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
					}		
                    offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    

                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;           
                    
					if( packLen1 > 0 ) {
						packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;
    
                case 8:
                    id                  = pa->pac_Id;
                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;

                    ib = ib->ib_Next;
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
					if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
					}

                    offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    
    
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;

					if( packLen1 > 0 ) {
	                    packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;

                case 10:
                    id                  = pa->pac_Id;
                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;

                    ib = ib->ib_Next;
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
    				if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
					}

                    offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    
               
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;
                    
					if( packLen1 > 0 ) {
						packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}	
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;

                case 12:
                    id                  = pa->pac_Id;
                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;

                    ib = ib->ib_Next;
//					printf("BUF_BYTES(ib): %d, sizeof(struct Packet) - bytesLeft: %d\n",
//							BUF_BYTES(ib),     sizeof(struct Packet) - bytesLeft);
    				if( BUF_BYTES(ib) < sizeof(struct Packet) - bytesLeft ) {
						return;
					}

                    offset = -bytesLeft;

                    pa = TO_PACKET(ib, offset);    
               
                    packLen1            = pa->pac_Length;
                    
					if( packLen1 > 0 ) {
						packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}	
					}
		            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
                    break;

                case 14:
                    id                  = pa->pac_Id;
                    packType            = pa->pac_Type;
                    packFlags           = pa->pac_Flags;
                    connId              = pa->pac_ConnId;
                    packId              = pa->pac_PackId;
					checksum			= pa->pac_Checksum;
                    packLen1            = pa->pac_Length;

					if( packLen1 > 0 ) {

						if( ib->ib_Next == NULL || BUF_BYTES(ib) < BUFFER_SIZE ) {
							return;
						}

	                    ib = ib->ib_Next;

    	                offset = -bytesLeft;
                
        	            pa = TO_PACKET(ib, offset);    

            	        packData1           = TO_DATA1(pa);

						if( BUF_BYTES(ib) + offset < packLen1 ) {
							return;
						}

			            APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
					}
                    break;                                        

    			default:        
                    printf("Bork bork bork! %d - %d\n", BUF_BYTES(ib), offset);            
                    return;
            }

        } else {
            id                  = pa->pac_Id;
            packType            = pa->pac_Type;
            packFlags           = pa->pac_Flags;
            connId              = pa->pac_ConnId;
            packId              = pa->pac_PackId;
			checksum			= pa->pac_Checksum;
            packLen1            = pa->pac_Length;

			if( packLen1 > 0 ) {
	            packData1           = TO_DATA1(pa);    

    	        if( bytesLeft - sizeof(struct Packet) < packLen1 ) {

					if( BUF_BYTES(ib) < BUFFER_SIZE ) {
						// Buffer not full	
						return;
					}

	           	    if( ib->ib_Next == NULL ) {
    	                // Not enough space left for the packet, and no more buffers.
       		            return;
   	        	    }

               		packLen2 = (sizeof(struct Packet) + packLen1) - bytesLeft;
	           	    packLen1 = bytesLeft - sizeof(struct Packet);
                
    	   	        ib = ib->ib_Next;
   	    	        offset = -bytesLeft;
	
    	            packData2 = ib->ib_Buf->b_Data;
					if( packLen1 + packLen2 > BUFFER_SIZE ) {
						printf("Invalid packet size %d\n", packLen1 + packLen2);
						goto badData;
					}

					if( BUF_BYTES(ib) < packLen2 ) {
						return;
					}

	                APB_IncrementStat(ST_IP_SPLIT_COUNT, 1);
				}
           	}
        }

        if( id != PACKET_ID ) {
			printf("Invalid packet id %ld\n", id);
			goto badData;
        }

		if( packLen1 + packLen2 > BUFFER_SIZE ) {
			printf("Invalid packet size %d\n", packLen1 + packLen2);
			goto badData;
	    }

		typeAndFlags = (((UWORD)packType) << 8) + packFlags;
		dataLen = packLen1 + packLen2;

		sum = APB_AddToChecksum(0, (UWORD *)&id, 4);
		sum = APB_AddToChecksum(sum, &typeAndFlags, 2);
		sum = APB_AddToChecksum(sum, &connId, 2);
		sum = APB_AddToChecksum(sum, &packId, 2);
		sum = APB_AddToChecksum(sum, &checksum, 2);
		sum = APB_AddToChecksum(sum, &dataLen, 2);

		if(packLen1 > 0 ) { 
			dataLen = packLen1 + (packLen1 & 1);
			sum = APB_AddToChecksum(sum, (UWORD *)packData1, dataLen);
			printf("length 1: %d\n", dataLen);
			if( packLen2 > 0 ) {
				dataLen =  packLen2 + (packLen2 & 1);
				sum = APB_AddToChecksum(sum, (UWORD *)packData2, dataLen);
				printf("length 2: %d\n", dataLen);
			}
		}

		sum = APB_CompleteChecksum(sum); 
		if( sum != 0xFFFF ) {
			printf("Invalid checksum: %x, headIb->ib_Offset: %d, ib->ib_Offset: %d, packLen1: %d, packLen2: %d, bytesLeft: %d\n", sum, headIb->ib_Offset, ib->ib_Offset, packLen1, packLen2, bytesLeft);
//			goto badData;
		}

        if( ! (ip = APB_AllocObject(pr->pr_ObjPool, OT_IN_PACKET) ) ) {
            APB_IncrementStat(ST_IP_ALLOC_FAILURES, 1);
            break;
        }

        APB_IncrementStat(ST_IP_ALLOCATED, 1);

        ip->ip_InBuf = headIb;
        ip->ip_Type = packType;
        ip->ip_Flags = packFlags;
        ip->ip_ConnId = connId;
        ip->ip_PackId = packId;
        ip->ip_Data1Length = packLen1;
        ip->ip_Data1 = packData1;
        ip->ip_Data2Length = packLen2;
        ip->ip_Data2 = packData2;
        ip->ip_Offset = 0;

        headIb->ib_PacketCount++;
        if( ib != headIb ) {
            ib->ib_PacketCount++;
        }
           
        AddTail(PR_QUEUE(pr), (struct Node *)ip);
        pr->pr_QueueSize++;
        pr->pr_InBuf = ib;

        APB_IncrementStat(ST_Q_INCOMING_SIZE, 1);

        offset += sizeof(struct Packet) + packLen1 + packLen2;

        if( packFlags & PF_PAD_BYTE ) {
            offset++;
        }

		APB_IncrementStat(ST_BYTES_RECEIVED, offset - ib->ib_Offset);            

        ib->ib_Offset = offset;

        if( ib->ib_Offset == BUF_BYTES(ib) ) {
            ib = ib->ib_Next;
        }
    }
	return;

badData:
	if(  headIb && headIb->ib_Offset < BUFFER_SIZE ) {
		headIb->ib_Offset += 2;
	}

	APB_IncrementStat(ST_IP_INVALID_DATA, 1);
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

