
#ifndef __APB_BUFFER_H__
#define __APB_BUFFER_H__

#define BUFFER_SIZE 4096;
#define MIN_BUFFERS 8
#define MAX_BUFFERS 16

struct Buffer
{
    struct MinNode     b_Node;
    struct BufferPool *b_Pool;
    UWORD              b_Offset;
    UWORD              b_UsageCount;
    UBYTE              b_Data[BUFFER_SIZE];
};

struct BufferRef
{
    struct MinNode  br_Node;
    UWORD           br_Offset;
    UWORD           br_Length;
    UWORD           br_Pos;
    struct Buffer*  br_Buf1;
    struct Buffer*  br_Buf2;
};

struct BufferPool
{
    struct MinList  bp_Buffers;
    struct MinList  bp_BufferRefs;
    UWORD           bp_BufferCount;
    UWORD           bp_BuffersInUse;
    UWORD           bp_BufferRefCount;
    UWORD           bp_BufferRefsInUse;
};

#define BUF_TO_DATA(buf) ((UBYTE*)(&buf->b_Data))

#define DATA_TO_BUF(data) ((struct Buffer *)(data - (sizeof(Buffer) - BUFFER_SIZE)))


#define BUFREF_IS_EMPTY(br) (br->br_Pos == br->br_Length)

#define BUFREF_POS(br) (br->br_Offset + br->br_Pos > BUFFER_SIZE ? (br->br_Pos - (BUFFER_SIZE - br->br_Offset)) : (br->br_Offset + br->br_Pos))

#define BUFREF_CURRENT_BUFFER(br) (br->br_Buf2 ? br->br_Buf2 : br->br_Buf1)

#define BUFREF_TO_DATA(br) (BUFREF_CURRENT_DATA(br) + BUFREF_POS(br))

#define BUFREF_MAX_READ_SIZE(br) (br->br_Offset + br->br_Pos < BUFFER_SIZE ? (BUFFER_SIZE - br->br_Offset - br->br_Pos) : (br->br_Length - (BUFFER_SIZE - br->br_Offset) - br->br_Pos - (BUFFER_SIZE - br->br_Offset))

VOID APB_InitBufferPool(struct BufferPool *bp);

VOID APB_FreeBufferPool(struct BufferPool *bp);

struct Buffer *APB_AllocBuffer(struct BufferPool *bp);

VOID APB_ReleaseBuffer(struct Buffer *buf);

struct BufferRef *APB_AllocBufferRef(struct Buffer *buf, UWORD length);

VOID APB_ReleaseBufferRef(struct BufferRef *buf);

UWORD APB_WriteToBuffer(struct Buffer *buf, VOID *data, UWORD length);

UWORD APB_ReadFromBuffer(struct Buffer *buf, VOID *data, UWORD length);

UWORD APB_ReadFromBufferRef(struct BufferRef *br, VOID *data, UWORD length);

UWORD APB_IncrementBufferRef(struct BufferRef *br, UWORD length);

#endif // __APB_BUFFER_H__

