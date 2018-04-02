
#ifndef __APB_PACKET_READER_H__
#define __APB_PACKET_READER_H__

#include <exec/types.h>
#include <exec/lists.h>

#include "protocol.h"
#include "memory.h"
#include "objectpool.h"

typedef APTR BufferRef;

typedef APTR PacketReader;

struct PacketRef
{
    struct MinNode       pr_Node;
    BufferRef   	     pr_BufRef;
    UBYTE                pr_Type;
	UBYTE                pr_Flags;
	UWORD                pr_ConnId;
    UWORD				 pr_PackId;
    UWORD                pr_Offset;
    UWORD                pr_Data1Length;
    UWORD                pr_Data2Length;
    UBYTE               *pr_Data1;
    UBYTE               *pr_Data2;
};

// Packet Reader

PacketReader APB_CreatePacketReader(MemoryPool memPool, ObjectPool objPool);

VOID APB_DestroyPacketReader(PacketReader pr);

VOID APB_ProcessBuffer(PacketReader pr, struct Buffer *buf);

VOID APB_ReleasePacketRef(struct PacketRef *p);

UWORD APB_ReaderQueueSize(PacketReader pr);

struct PacketRef *APB_DequeuePacketRef(PacketReader pr);

#endif // __APB_BUFFER_H__