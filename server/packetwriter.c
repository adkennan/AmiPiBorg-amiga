
#include "packetwriter.h"
#include "buffer.h"
#include "stats.h"
#include "memory.h"

#include <exec/lists.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

//#include <stdio.h>

struct PacketWriter
{
    MemoryPool          pw_MemPool;
    ObjectPool          pw_ObjPool;
    struct MinList      pw_Queue;
    struct Buffer      *pw_Buf;
    UWORD               pw_QueueSize;
    UWORD               pw_NextPackId;
};

#define PW_QUEUE(pw) ((struct List *)&pw->pw_Queue)

// Packet Writer

PacketWriter APB_CreatePacketWriter(MemoryPool memPool, ObjectPool objPool)
{
    struct PacketWriter *pw;

    if( ! (pw = APB_AllocMem(memPool, sizeof(struct PacketWriter) ) ) ) {

        return NULL;
    }

    pw->pw_MemPool = memPool;
    pw->pw_ObjPool = objPool;
    pw->pw_Buf = NULL;
    pw->pw_QueueSize = 0;
    pw->pw_NextPackId = 1;
    NewList(PW_QUEUE(pw));

    return pw;
}

VOID APB_DestroyPacketWriter(PacketWriter packetWriter)
{
    struct PacketWriter *pw = (struct PacketWriter *)packetWriter;

    while( pw->pw_QueueSize > 0 ) {

        APB_ReleaseBuffer((struct Buffer *)APB_DequeueBuffer(pw));
    }

    APB_FreeMem(pw->pw_MemPool, pw, sizeof(struct PacketWriter));
}

struct Buffer *APB_DequeueBuffer(PacketWriter packetWriter)
{
    struct PacketWriter *pw = (struct PacketWriter *)packetWriter;

    if( pw->pw_QueueSize == 0 ) {

        return NULL;
    }

    pw->pw_QueueSize--;
    
    if( pw->pw_QueueSize == 0 ) {
        // That was the last buffer.
        pw->pw_Buf = NULL;
    }
    
    APB_IncrementStat(ST_Q_OUTGOING_SIZE, -1);

    return (struct Buffer *)RemHead(PW_QUEUE(pw));
}

UWORD APB_WriterQueueSize(PacketWriter packetWriter)
{
    struct PacketWriter *pw = (struct PacketWriter *)packetWriter;

    return pw->pw_QueueSize;
}

struct Packet *APB_AllocPacket(PacketWriter packetWriter, UWORD length)
{
    struct PacketWriter *pw = (struct PacketWriter *)packetWriter;

    return APB_AllocPacketWithId(pw, length, pw->pw_NextPackId++);
}

struct Packet *APB_AllocPacketWithId(PacketWriter packetWriter, UWORD length, UWORD packId)
{
    struct PacketWriter *pw = (struct PacketWriter *)packetWriter;

    struct Buffer *buf;
    struct Packet *p;

    if( length > BUFFER_SIZE ) {
        // Packet too big!
        return NULL;
    }

    if( length < sizeof(struct Packet) ) {
        // Packet length too small.

        return NULL;
    }

    buf = pw->pw_Buf;

    if( !buf // No current buffer
        || (buf->b_Offset + length + 1) >= BUFFER_SIZE ) // No room left in buffer

    {
        if( ! (buf = APB_AllocateBuffer(pw->pw_ObjPool) ) ) {
            // Can't get a buffer!
            return NULL;
        }

        pw->pw_QueueSize++;
        pw->pw_Buf = buf;

        AddTail(PW_QUEUE(pw), BUF_NODE(buf));

        APB_IncrementStat(ST_Q_OUTGOING_SIZE, 1);
    }

    p = (struct Packet *)APB_PointerAdd(buf->b_Data, buf->b_Offset);

    p->pac_Id = PACKET_ID;
    p->pac_PackId = packId;
    p->pac_Length = length - sizeof(struct Packet);

    buf->b_Offset += length;

	if( length % 2 == 1 ) {
		p->pac_Flags = PF_PAD_BYTE;
		buf->b_Offset++;
	}

    APB_IncrementStat(ST_PAC_SENT, 1);

    return p;
}


