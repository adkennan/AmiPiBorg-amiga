#ifndef __APB_BUFFER_H__
#define __APB_BUFFER_H__

#include <exec/types.h>
#include <exec/lists.h>

#include "objectpool.h"

#define BUFFER_SIZE 256

#define MIN_BUFFERS 8
#define MAX_BUFFERS 16

struct Buffer
{
    struct MinNode      b_Node;
    ObjectPool          b_ObjPool;
    UWORD               b_Offset;
    UBYTE               b_Data[BUFFER_SIZE];
};

#define BUF_CURR(buf) (APB_PointerAdd(buf->b_Data, buf->b_Offset))
#define BUF_AVAIL(buf) (BUFFER_SIZE - buf->b_Offset)
#define BUF_FULL(buf) (BUFFER_SIZE == buf->b_Offset)

#define BUF_NODE(buf) ((struct Node *)&buf->b_Node)

struct Buffer *APB_AllocateBuffer(ObjectPool objPool);

VOID APB_ReleaseBuffer(struct Buffer *buf);


#endif // _APB_BUFFER_H__

