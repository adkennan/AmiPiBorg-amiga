
#ifndef __APB_OBJECT_POOL_H__
#define __APB_OBJECT_POOL_H__

#include <exec/types.h>

#include "memory.h"

enum ObjectTypes
{
    OT_BUFFER    = 100,
    OT_IN_PACKET = 200,
    OT_IN_BUFFER = 300,
    OT_CONNECTION = 400,
	OT_OUT_PACKET = 500,
	OT_OUT_BUFFER = 600
};

typedef APTR ObjectPool;

ObjectPool APB_CreateObjectPool(MemoryPool memPool);

VOID APB_DestroyObjectPool(ObjectPool objPool);

BOOL APB_TypeRegistered(ObjectPool pool, UWORD id);

VOID APB_RegisterObjectType(ObjectPool  objPool
						  , UWORD       id
						  , UWORD       size
    					  , UWORD       minInstances
						  , UWORD       maxInstances);

APTR APB_AllocObject(ObjectPool objPool, UWORD id);

VOID APB_FreeObject(ObjectPool objPool, UWORD id, APTR obj);

#endif // __APB_OBJECT_POOL_H__