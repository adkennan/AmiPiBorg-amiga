
#include "buffer.h"
#include "stats.h"
#include "log.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

struct Buffer *APB_AllocateBuffer(
    ObjectPool objPool)
{
    struct Buffer *buf;

    if(!APB_TypeRegistered(objPool, OT_BUFFER)) {
        APB_RegisterObjectType(objPool, OT_BUFFER, sizeof(struct Buffer), MIN_BUFFERS, MAX_BUFFERS);
    }

    if(!(buf = (struct Buffer *) APB_AllocObject(objPool, OT_BUFFER))) {

        LOG0(LOG_ERROR, "Unable to allocate buffer");

        APB_IncrementStat(ST_BUF_ALLOC_FAILURES, 1);
        return NULL;
    }

    LOG1(LOG_TRACE, "Buffer 0x%00000000x Allocated", buf);

    buf->b_ObjPool = objPool;
    buf->b_Offset = 0;

    APB_IncrementStat(ST_BUF_ALLOCATED, 1);

    return buf;
}

VOID APB_ReleaseBuffer(
    struct Buffer * buf)
{
    LOG1(LOG_TRACE, "Buffer 0x%00000000x Freed", buf);

    APB_FreeObject(buf->b_ObjPool, OT_BUFFER, buf);

    APB_IncrementStat(ST_BUF_ALLOCATED, -1);
}
