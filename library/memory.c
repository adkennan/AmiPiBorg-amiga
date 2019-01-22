
#include "lib.h"

#include <clib/exec_protos.h>

APTR APB_AllocMemInternal(
    APTR memPool,
    ULONG size)
{
    if(memPool == NULL) {

        return AllocMem(size, MEMF_CLEAR);

    } else {

        return AllocPooled(memPool, size);
    }
}

VOID APB_FreeMemInternal(
    APTR memPool,
    APTR memory,
    ULONG size)
{

    if(memPool == NULL) {

        FreeMem(memory, size);

    } else {

        FreePooled(memPool, memory, size);
    }
}

APTR __asm __saveds APB_AllocMem(
    register __a0 APTR ctx,
    register __d0 ULONG size)
{
    return APB_AllocMemInternal(((struct ApbContext *) ctx)->ctx_MemPool, size);
}

VOID __asm __saveds APB_FreeMem(
    register __a0 APTR ctx,
    register __a1 APTR memory,
    register __d0 ULONG size)
{
    APB_FreeMemInternal(((struct ApbContext *) ctx)->ctx_MemPool, memory, size);
}
