
#ifndef __APB_PACKET_WRITER_H__
#define __APB_PACKET_WRITER_H__

#include <exec/types.h>
#include <exec/lists.h>

#include "protocol.h"
#include "remote.h"

typedef APTR PacketWriter;

PacketWriter APB_CreatePacketWriter(
    APTR ctx);

VOID      APB_DestroyPacketWriter(
    PacketWriter packetWriter);

VOID      APB_WriteBuffer(
    PacketWriter packetWriter,
    Remote remote);

struct Packet *APB_AllocPacket(
    PacketWriter packetWriter,
    UWORD length);

struct Packet *APB_AllocPacketWithId(
    PacketWriter packetWriter,
    UWORD length,
    UWORD packId);

VOID      APB_ResendPacket(
    PacketWriter packetWriter,
    UWORD packId);

#endif // _APB_PACKET_WRITER_H__
