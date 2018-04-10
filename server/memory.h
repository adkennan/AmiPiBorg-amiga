
#ifndef __APB_MEMORY_H__
#define __APB_MEMORY_H__

#include <exec/types.h>

typedef APTR MemoryPool;

MemoryPool APB_CreatePool(VOID);

VOID APB_DestroyPool(MemoryPool pool);

VOID *APB_AllocMem(MemoryPool pool, ULONG size);

VOID APB_FreeMem(MemoryPool pool, VOID *memory, ULONG size);

APTR APB_PointerAdd(APTR ptr, LONG amt);

#endif // __APB_MEMORY_H__

