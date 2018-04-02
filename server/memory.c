
#include "memory.h"
#include "stats.h"

#include <exec/memory.h>

#include <clib/exec_protos.h>

#ifdef APB_DEBUG_MEM
#include <stdio.h>
#endif

struct Pool
{
    APTR p_MemPool;
};

VOID APB_MemIncrementStats(LONG size)
{
    APB_IncrementStat(ST_MEM_ALLOC_COUNT, 1);
    APB_IncrementStat(ST_MEM_TOTAL_ALLOCATED, size);
    APB_IncrementStat(ST_MEM_CURRENT_ALLOCATED, size);

#ifdef APB_DEBUG_MEM
    printf("+%ld bytes\n", size);
#endif
}

VOID APB_MemDecrementStats(LONG size)
{
    APB_IncrementStat(ST_MEM_FREE_COUNT, 1);
    APB_IncrementStat(ST_MEM_TOTAL_FREED, size);
    APB_IncrementStat(ST_MEM_CURRENT_ALLOCATED, -size);

#ifdef APB_DEBUG_MEM
    printf("-%ld bytes\n", size);
#endif
}

MemoryPool APB_CreatePool(VOID)
{
    APTR mp;
    struct Pool *p;

    if( ! (mp = CreatePool(MEMF_ANY, 4096, 2048) ) ) {
        return NULL;
    }

    if( ! (p = AllocPooled(mp, sizeof(struct Pool) ) ) ) {
        DeletePool(mp);
        return NULL;
    }

    APB_MemIncrementStats(sizeof(struct Pool));
    
    p->p_MemPool = mp;

    return p;
}

VOID APB_DestroyPool(MemoryPool pool)
{    
    APTR mp;
    struct Pool *p = (struct Pool *)pool;

    mp = p->p_MemPool;

    FreePooled(mp, pool, sizeof(struct Pool));

    APB_MemDecrementStats(sizeof(struct Pool));

    DeletePool(mp);
}

VOID *APB_AllocMem(MemoryPool pool, ULONG size)
{
    struct Pool *p = (struct Pool *)pool;
    VOID *mem;

    if( ! ( mem = AllocPooled(p->p_MemPool, size) ) ) {
        return NULL;
    }
    
    APB_MemIncrementStats((LONG)size);

    return mem;
}

VOID APB_FreeMem(MemoryPool pool, VOID *memory, ULONG size)
{
    struct Pool *p = (struct Pool *)pool;

    FreePooled(p->p_MemPool, memory, size);

    APB_MemDecrementStats((LONG)size);
}
