
#include "buffer.h"
#include "server.h"

#define BUF_LIST(bp) ((struct List *)&bp->bp_Buffers)
#define BUFREF_LIST(bp) ((struct List *)&bp->bp_BufferRefs)

#define MIN(x,y) (x > y ? y : x)

VOID APB_InitBufferPool(struct BufferPool *bp)
{
    UWORD ix;
    struct Buffer *buf;
    struct BufferRef *br;

    NewList(BUF_LIST(bp));
    

    for( ix = 0; ix < MIN_BUFFERS; ix++ ) {
        
        if( buf = ALLOC_STRUCT(Buffer) ) {

            buf->b_Pool = bp;

            AddTail(BUF_LIST(bp), TO_NODE(buf));
        }        
        if( br = ALLOC_STRUCT(BufferRef) ) {

            AddTail(BUFREF_LIST(bp), TO_NODE(br));
        }
    }
}

VOID APB_FreeBufferPool(struct BufferPool *bp)
{
    struct Buffer *buf;
    
    while( !IsListEmpty(BUF_LIST(bp)) ) {
        buf = (struct Buffer *)RemTail(BUF_LIST(bp));
        APB_FreeMem(buf, sizeof(struct Buffer));
    }
}

struct Buffer *APB_AllocBuffer(struct BufferPool *bp)
{
    struct Buffer *buf;

    if( IsListEmpty(BUF_LIST(bp)) ) {

        if( bp->bp_BufferCount == MAX_BUFFERS ) {

            // UH OH... no more buffers!
            return NULL;
        }

        if( !(buf = ALLOC_STRUCT(Buffer)) ) {

            // UH OH... no more RAM!
            return NULL;
        }

        bp->bp_BufferCount++;

        buf->b_Pool = bp;

    } else {

        buf = (struct Buffer *)RemHead(BUF_LIST(bp));
    }

    buf->b_Offset = 0;
    buf->b_UsageCount = 0;

    bp->bp_BuffersInUse++;

    return buf;       
}

VOID APB_ReleaseBuffer(struct Buffer *buf)
{
    AddTail(BUF_LIST(buf->b_Pool), TO_NODE(buf));

    buf->b_Pool->bp_BuffersInUse--;
}

struct BufferRef *APB_AllocBufferRef(struct Buffer *buf, UWORD length)
{
    struct BufferRef *br;
    
    if( IsListEmpty(BUFREF_LIST(buf->b_Pool)) ) {

        if( !(br = ALLOC_STRUCT(BufferRef)) ) {
    
            // OH OH... no more RAM!
            return NULL;
        }

        buf->b_Pool->bp_BufferRefCount++;
    } else {
        br = (struct BufferRef *)RemHead(BUFREF_LIST(buf->b_Pool));
    }

    buf->b_UsageCount++;
    
    br->br_Buf1   = buf;
    br->br_Offset = buf->b_Offset;
    br->br_Pos    = 0;
    br->br_Length = length;

    if( br->br_Offset + length > BUFFER_SIZE ) {
        br->br_Buf2 = APB_AllocateBuffer(buf->b_Pool);
    } else {
        br->br_Buf2 = NULL;
    }

    buf->b_Pool->bp_BufferRefsInUse++;
}

VOID APB_ReleaseBufferRef(struct BufferRef *br)
{
    AddTail(BUFREF_LIST(br->br_Buffer->b_Pool), TO_NODE(br));

    br->br_Buf1->b_Pool->bp_BufferRefsInUse--;

    br->br_Buf1->b_UsageCount--;

    if( br->br_Buf1->b_UsageCount == 0 ) {

        APB_ReleaseBuffer(br->br_Buf1);
    }

    if( br->br_Buf2 ) {
        br->br_Buf2->b_UsageCount--;
    
        if( br->br_Buf2->b_UsageCount == 0 ) {

            APB_ReleaseBuffer(br->br_Buf2);
        }
    }
}

UWORD APB_WriteToBuffer(struct Buffer *buf, VOID *data, UWORD length)
{
    UWORD bytesToCopy = MIN(length, BUFFER_SIZE - buf->b_Offset);

    if( bytesToCopy <= 0 ) {
        return 0;
    }

    CopyMem(data, buf->b_Data + buf->b_Offset, length);
    buf->b_Offset += length;

    return bytesToCopy;
}

UWORD APB_ReadFromBuffer(struct Buffer *buf, VOID *data, UWORD length)
{
    UWORD bytesToCopy = MIN(length, BUFFER_SIZE - buf->b_Offset);

    if( bytesToCopy <= 0 ) {
        return 0;
    }

    CopyMem(buf->b_Data + buf->b_Offset, data, length);
    buf->b_Offset += length;

    return bytesToCopy;
}

UWORD APB_ReadFromBufferRef(struct BufferRef *br, VOID *data, UWORD length)
{
    UWORD bytesToCopy, bytesCopied;

    bytesToCopy = MIN(length, BUFREF_MAX_READ_SIZE(br))

    CopyMem(BUFREF_TO_DATA(br), data, bytesToCopy);
    
    br->br_Pos += bytesToCopy;
    bytesCopied = bytesToCopy;

    length -= bytesToCopy;
    if( length > 0 ) {

        bytesToCopy = MIN(length, BUFREF_MAX_READ_SIZE(br))

        CopyMem(BUFREF_TO_DATA(br), data, bytesToCopy);
    
        br->br_Pos += bytesToCopy;           
        bytesCopied += bytesToCopy;
    }

    return bytesCopied;
}

VOID APB_IncrementBufferRef(struct BufferRef *br, UWORD length)
{
    br->br_Pos += length;
}

