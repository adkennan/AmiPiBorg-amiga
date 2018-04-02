
#ifndef __APB_REMOTE_H__
#define __APB_REMOTE_H__

#include "memory.h"
#include "buffer.h"

#include <exec/types.h>

typedef APTR Remote;

Remote APB_CreateRemote(MemoryPool memPool, ObjectPool objPool);

VOID APB_DestroyRemote(Remote remote);

BOOL APB_OpenRemote(Remote remote);

VOID APB_CloseRemote(Remote remote);

ULONG APB_GetSigBits(Remote remote);

VOID APB_HandleSignal(Remote remote, ULONG sigBits);

BOOL APB_CanSendToRemote(Remote remote);

VOID APB_SendToRemote(Remote remote, struct Buffer *buf);

BOOL APB_CanReceiveFromRemote(Remote remote);

struct Buffer *APB_ReceiveFromRemote(Remote remote);

#endif // __APB_REMOTE_H__