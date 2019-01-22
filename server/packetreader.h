
#ifndef __APB_PACKET_READER_H__
#define __APB_PACKET_READER_H__

#include <exec/types.h>
#include <exec/lists.h>

#include "protocol.h"

typedef APTR InBuffer;

typedef APTR PacketReader;

struct InPacket {
    struct MinNode ip_Node;
    InBuffer  ip_InBuf;
    UBYTE     ip_Type;
    UBYTE     ip_Flags;
    UWORD     ip_ConnId;
    UWORD     ip_PackId;
    UWORD     ip_Offset;
    UWORD     ip_Data1Length;
    UWORD     ip_Data2Length;
    UBYTE    *ip_Data1;
    UBYTE    *ip_Data2;
};

// Packet Reader

PacketReader APB_CreatePacketReader(
    APTR ctx);

VOID      APB_DestroyPacketReader(
    PacketReader pr);

VOID      APB_ProcessBuffer(
    PacketReader pr,
    struct Buffer *buf);

VOID      APB_ReleaseInPacket(
    struct InPacket *p);

UWORD     APB_ReaderQueueSize(
    PacketReader pr);

struct InPacket *APB_DequeueInPacket(
    PacketReader pr);

UWORD     APB_InPacketToBuffer(
    struct InPacket *ip,
    APTR buf);

#endif // __APB_BUFFER_H__
