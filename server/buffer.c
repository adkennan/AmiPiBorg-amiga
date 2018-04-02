
#include "buffer.h"
#include "stats.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

UWORD APB_BufferLength(struct Buffer *buf)
{
    return buf->b_Offset;
}

UBYTE *APB_BufferData(struct Buffer *buf)
{
    return buf->b_Data; 
}

struct Buffer *APB_AllocateBuffer(ObjectPool objPool)
{
    struct Buffer *buf;

    if( ! APB_TypeRegistered(objPool, OT_BUFFER) ) {
        APB_RegisterObjectType(objPool, OT_BUFFER, sizeof(struct Buffer), MIN_BUFFERS, MAX_BUFFERS);
    }

    if( ! (buf = (struct Buffer *)APB_AllocObject(objPool, OT_BUFFER) ) ) {

        APB_IncrementStat(ST_BUF_ALLOC_FAILURES, 1);
        return NULL;
    }

    buf->b_ObjPool = objPool;
    buf->b_Offset = 0;    

    APB_IncrementStat(ST_BUF_ALLOCATED, 1);

    return buf;
}

VOID APB_ReleaseBuffer(struct Buffer *buf)
{
    APB_FreeObject(buf->b_ObjPool, OT_BUFFER, buf);

    APB_IncrementStat(ST_BUF_ALLOCATED, -1);
}
