
#include "buffer.h"
#include "objtype.h"

#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

struct Buffer *APB_AllocateBuffer(
    APTR ctx)
{
    struct Buffer *buf;

    if(!APB_TypeRegistered(ctx, OT_BUFFER)) {
        APB_RegisterObjectType(ctx, OT_BUFFER, "Buffer", sizeof(struct Buffer), MIN_BUFFERS, MAX_BUFFERS);
    }

    if(!APB_TypeRegistered(ctx, OT_BUFFER)) {
        return NULL;
    }

    if(!(buf = (struct Buffer *) APB_AllocObject(ctx, OT_BUFFER))) {

//        LOG0(ctx, LOG_ERROR, "Unable to allocate buffer");

//        APB_IncrementStat(ST_BUF_ALLOC_FAILURES, 1);
        return NULL;
    }

    LOG1(ctx, LOG_TRACE, "Buffer 0x%00000000x Allocated", buf);

    buf->b_Ctx = ctx;
    buf->b_Offset = 0;

//    APB_IncrementStat(ST_BUF_ALLOCATED, 1);

    return buf;
}

VOID APB_ReleaseBuffer(
    struct Buffer * buf)
{
    LOG1(buf->b_Ctx, LOG_TRACE, "Buffer 0x%00000000x Freed", buf);

    APB_FreeObject(buf->b_Ctx, OT_BUFFER, buf);

//    APB_IncrementStat(ST_BUF_ALLOCATED, -1);
}
