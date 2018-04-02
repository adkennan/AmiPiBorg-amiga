#ifndef __APB_CONNECTION_H__
#define __APB_CONNECTION_H__

#include "objectpool.h"
#include "packetreader.h"
#include "packetwriter.h"

#include "amipiborg.h"

#include <exec/types.h>

typedef APTR Connection;

enum ConnectionState 
{
    CS_CONNECTING,
    CS_CONNECTED,
    CS_DISCONNECTING,
    CS_DISCONNECTED,
	CS_NO_HANDLER
};

Connection APB_CreateConnection(struct List *cnns, ObjectPool objectPool, UWORD connId, PacketWriter packetWriter);

VOID APB_DestroyConnection(Connection cnn);

Connection APB_FindConnection(struct List *cnns, UWORD id);

VOID APB_HandleConnectionPacket(Connection cnn, struct PacketRef *pr);

VOID APB_HandleClientRequest(Connection cnn, struct APBRequest *req);

UWORD APB_GetConnectionState(Connection cnn);

UWORD APB_GetId(Connection cnn);

#endif // __APB_CONNECTION_H__