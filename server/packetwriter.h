
#ifndef __APB_PACKET_WRITER_H__
#define __APB_PACKET_WRITER_H__

#include <exec/types.h>

#include "protocol.h"
#include "memory.h"
#include "objectpool.h"

typedef APTR PacketWriter;

PacketWriter APB_CreatePacketWriter(MemoryPool memPool, ObjectPool objPool);

VOID APB_DestroyPacketWriter(PacketWriter packetWriter);

struct Buffer *APB_DequeueBuffer(PacketWriter packetWriter);

UWORD APB_WriterQueueSize(PacketWriter packetWriter);

struct Packet *APB_AllocPacket(PacketWriter packetWriter, UWORD length);

struct Packet *APB_AllocPacketWithId(PacketWriter packetWriter, UWORD length, UWORD packId);


#endif // _APB_PACKET_WRITER_H__