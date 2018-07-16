
#include "lib.h"

#include <clib/exec_protos.h>

VOID     *APB_AllocMem(
    APTR memPool,
    ULONG memSize)
{
    if(memPool == NULL) {

        return AllocMem(memSize, MEMF_CLEAR);

    } else {

        return AllocPooled(memPool, memSize);
    }
}

VOID APB_FreeMem(
    APTR memPool,
    VOID * mem,
    ULONG memSize)
{
    if(memPool == NULL) {

        FreeMem(mem, memSize);

    } else {

        FreePooled(memPool, mem, memSize);
    }
}
