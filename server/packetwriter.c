
#include "packetwriter.h"
#include "buffer.h"
#include "stats.h"
#include "memory.h"
#include "protocol.h"
#include "remote.h"
#include "log.h"

#include "remote_protos.h"
#include "remote_pragmas.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

#define RECENT_PACKETS_TO_KEEP 10

struct OutBuffer {
    struct MinNode ob_Node;
    struct Buffer *ob_Buf;
    UWORD     ob_Offset;
    UWORD     ob_PacketCount;
    BOOL      ob_CanRelease;
};

struct OutPacket {
    struct MinNode op_Node;
    struct OutBuffer *op_OutBuf;
    struct Packet *op_Pac;
    BOOL      op_Sent;
};

struct PacketWriter {
    MemoryPool pw_MemPool;
    ObjectPool pw_ObjPool;
    struct MinList pw_Queue;
    struct MinList pw_RecentPackets;
    struct OutBuffer *pw_OutBuf;
    UWORD     pw_RecentCount;
    UWORD     pw_BufCount;
    UWORD     pw_NextPackId;
};

#define PW_QUEUE(pw) ((struct List *)&pw->pw_Queue)
#define PW_PACKS(pw) ((struct List *)&pw->pw_RecentPackets)

PacketWriter APB_CreatePacketWriter(
    MemoryPool memPool,
    ObjectPool objPool)
{
    struct PacketWriter *pw;

    if(!APB_TypeRegistered(objPool, OT_OUT_BUFFER)) {
        APB_RegisterObjectType(objPool, OT_OUT_BUFFER, sizeof(struct OutBuffer), MIN_BUFFERS, MAX_BUFFERS);
    }

    if(!APB_TypeRegistered(objPool, OT_OUT_PACKET)) {
        APB_RegisterObjectType(objPool, OT_OUT_PACKET, sizeof(struct OutPacket), MIN_BUFFERS * 10, MAX_BUFFERS * 10);
    }

    if(!(pw = APB_AllocMem(memPool, sizeof(struct PacketWriter)))) {
        LOG0(LOG_ERROR, "Unable to allocate memory for PacketWriter");
        return NULL;
    }

    pw->pw_MemPool = memPool;
    pw->pw_ObjPool = objPool;
    pw->pw_OutBuf = NULL;
    pw->pw_RecentCount = 0;
    pw->pw_BufCount = 0;
    pw->pw_NextPackId = 1;
    NewList(PW_QUEUE(pw));
    NewList(PW_PACKS(pw));

    return pw;
}

VOID APB_ReleaseOutBuffer(
    struct PacketWriter * pw,
    struct OutBuffer * ob)
{
    LOG1(LOG_TRACE, "Release OutBuffer 0x%x", ob);
    APB_ReleaseBuffer(ob->ob_Buf);
    APB_FreeObject(pw->pw_ObjPool, OT_OUT_BUFFER, ob);
}

VOID APB_ReleaseOutPacket(
    struct PacketWriter *pw,
    struct OutPacket *op)
{
    LOG1(LOG_TRACE, "Release OutPacket 0x%x", op);

    op->op_OutBuf->ob_PacketCount--;
    if(op->op_OutBuf->ob_PacketCount == 0 && op->op_OutBuf->ob_CanRelease) {
        APB_ReleaseOutBuffer(pw, op->op_OutBuf);
    }
    APB_FreeObject(pw->pw_ObjPool, OT_OUT_PACKET, op);
}

VOID APB_DestroyPacketWriter(
    PacketWriter packetWriter)
{
    struct PacketWriter *pw = (struct PacketWriter *) packetWriter;

    LOG0(LOG_TRACE, "Destroy Packet Writer");

    while(!IsListEmpty(PW_QUEUE(pw))) {

        APB_ReleaseOutBuffer(pw, (struct OutBuffer *) RemHead(PW_QUEUE(pw)));
    }

    while(!IsListEmpty(PW_PACKS(pw))) {

        APB_ReleaseOutPacket(pw, (struct OutPacket *) RemHead(PW_PACKS(pw)));
    }

    APB_FreeMem(pw->pw_MemPool, pw, sizeof(struct PacketWriter));
}

struct Packet *APB_AllocPacket(
    PacketWriter packetWriter,
    UWORD length)
{
    struct PacketWriter *pw = (struct PacketWriter *) packetWriter;

    return APB_AllocPacketWithId(pw, length, pw->pw_NextPackId++);
}

struct Packet *APB_AllocPacketWithId(
    PacketWriter packetWriter,
    UWORD length,
    UWORD packId)
{
    struct PacketWriter *pw = (struct PacketWriter *) packetWriter;

    struct OutBuffer *ob;
    struct OutPacket *op;
    struct OutPacket *op2;

    LOG2(LOG_TRACE, "Allocate Packet Id = %d, Length = %d", packId, length);

    if(length > BUFFER_SIZE) {
        // Packet too big!
        LOG1(LOG_ERROR, "Packet size %d too large", length);
        return NULL;
    }

    if(length < sizeof(struct Packet)) {
        // Packet length too small.
        LOG1(LOG_ERROR, "Packet size %d too small", length);
        return NULL;
    }

    ob = pw->pw_OutBuf;

    if(!ob                      // No current buffer
       || (ob->ob_Buf->b_Offset + length + 1) >= BUFFER_SIZE)   // No room left in buffer

    {
        LOG0(LOG_TRACE, "Allocate New OutBuffer");

        if(!(ob = (struct OutBuffer *) APB_AllocObject(pw->pw_ObjPool, OT_OUT_BUFFER))) {
            return NULL;
        }

        if(!(ob->ob_Buf = APB_AllocateBuffer(pw->pw_ObjPool))) {
            return NULL;
        }

        ob->ob_Offset = 0;
        ob->ob_PacketCount = 0;
        ob->ob_CanRelease = FALSE;

        pw->pw_OutBuf = ob;

        AddTail(PW_QUEUE(pw), (struct Node *) ob);
        pw->pw_BufCount++;

        APB_IncrementStat(ST_Q_OUTGOING_SIZE, 1);
    }

    if(!(op = (struct OutPacket *) APB_AllocObject(pw->pw_ObjPool, OT_OUT_PACKET))) {
        LOG0(LOG_ERROR, "Failed to allocate outgoing packet");
        return NULL;
    }

    op->op_OutBuf = ob;
    op->op_Pac = (struct Packet *) APB_PointerAdd(ob->ob_Buf->b_Data, ob->ob_Buf->b_Offset);
    op->op_Sent = FALSE;

    op->op_Pac->pac_Id = PACKET_ID;
    op->op_Pac->pac_PackId = packId;
    op->op_Pac->pac_Flags = 0;
    op->op_Pac->pac_Length = length - sizeof(struct Packet);
    op->op_Pac->pac_Checksum = 0;
    ob->ob_Buf->b_Offset += length;
    ob->ob_PacketCount++;

    if(length % 2 == 1) {
        op->op_Pac->pac_Flags = PF_PAD_BYTE;
        ob->ob_Buf->b_Offset++;
        *((BYTE *) APB_PointerAdd(ob->ob_Buf->b_Data, ob->ob_Buf->b_Offset)) = 0;
    }

    while(pw->pw_RecentCount > RECENT_PACKETS_TO_KEEP) {
        op2 = (struct OutPacket *) PW_PACKS(pw)->lh_Head;
        if(!op2->op_Sent) {
            break;
        }

        Remove((struct Node *) op2);

        APB_ReleaseOutPacket(pw, op2);

        pw->pw_RecentCount--;
    }

    AddTail(PW_PACKS(pw), (struct Node *) op);
    pw->pw_RecentCount++;

    APB_IncrementStat(ST_PAC_SENT, 1);

    return op->op_Pac;
}

VOID APB_WriteBuffer(
    PacketWriter packetWriter,
    Remote remote)
{
    struct PacketWriter *pw = (struct PacketWriter *) packetWriter;
    UWORD     bc, ps;           //, ix;
    BYTE     *data;
    struct OutBuffer *ob;
    struct OutPacket *op;

    if(IsListEmpty(PW_QUEUE(pw))) {
        return;
    }

    ob = (struct OutBuffer *) PW_QUEUE(pw)->lh_Head;
    if(ob->ob_Offset < ob->ob_Buf->b_Offset) {

        for(op = (struct OutPacket *) PW_PACKS(pw)->lh_Head; op->op_Node.mln_Succ; op = (struct OutPacket *) op->op_Node.mln_Succ) {

            if(op->op_OutBuf == ob && !op->op_Sent) {

                LOG1(LOG_TRACE, "Sending Packet %d", op->op_Pac->pac_PackId);

                ps = sizeof(struct Packet) + op->op_Pac->pac_Length + (op->op_Pac->pac_Length % 2 == 1 ? 1 : 0);
                op->op_Pac->pac_Checksum = APB_CalculateChecksum((UWORD *) op->op_Pac, ps);
                op->op_Sent = TRUE;
            }
        }

        bc = ob->ob_Buf->b_Offset - ob->ob_Offset;
        data = APB_PointerAdd(ob->ob_Buf->b_Data, ob->ob_Offset);

        LOG1(LOG_DEBUG, "Write %d bytes", bc);

        if(APB_ShouldLog(LOG_TRACE)) {
            APB_LogMem(__FILE__, __LINE__, __FUNC__, LOG_TRACE, data, bc);
        }

        REM_Write(remote, data, bc);

        APB_IncrementStat(ST_BYTES_SENT, bc);

        ob->ob_Offset = ob->ob_Buf->b_Offset;
    }

    if(pw->pw_BufCount > 1) {

        ob = (struct OutBuffer *) PW_QUEUE(pw)->lh_Head;

        ob->ob_CanRelease = TRUE;
        Remove((struct Node *) ob);
        pw->pw_BufCount--;

        APB_IncrementStat(ST_Q_OUTGOING_SIZE, -1);
    }
}

VOID APB_ResendPacket(
    PacketWriter packetWriter,
    UWORD packId)
{
    struct PacketWriter *pw = (struct PacketWriter *) packetWriter;
    struct OutPacket *op;
    struct Packet *copy;

    for(op = (struct OutPacket *) PW_PACKS(pw)->lh_Head; op->op_Node.mln_Succ; op = (struct OutPacket *) op->op_Node.mln_Succ) {

        if(op->op_Pac->pac_Id == packId) {

            if(copy = APB_AllocPacketWithId(pw, sizeof(struct Packet) + op->op_Pac->pac_Length, packId)) {

                CopyMem(op->op_Pac, copy, sizeof(struct Packet) + op->op_Pac->pac_Length);

                copy->pac_Flags |= PF_RESEND;
                copy->pac_Checksum = 0;
            }

            break;
        }
    }
}
