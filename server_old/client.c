
#include "client.h"

#define REQ_Q(cnn) ((struct List *)cnn->c_RequestQueue)
#define MSG_Q(cnn) ((struct List *)cnn->c_ReadQueue)


VOID APB_ReplyToRead(struct Connection *cnn)
{
    struct BufferRef *br = NULL;
    struct Packet *p = NULL;
    struct Request *req = NULL;
    UWORD byteCount = 0;
    UBYTE *data;

    while( (! IsListEmpty(MSG_Q(cnn))) && (! IsListEmpty(REQ_Q(cnn))) ) {

        if( br == NULL ) {
            br = (struct BufferRef *)RemHead(MSG_Q(cnn));
        }

        if( req == NULL ) {
            req = (struct Request *)RemHead(REQ_Q(cnn));
            byteCount = 0;
        }
         
        byteCount += APB_ReadFromBufferRef(br, req->r_Data, req->r_Length);

        if( BUFREF_IS_EMPTY(br) ) {
            APB_ReleaseBufferRef(br);
            br = NULL;
        }

        if( byteCount == req->r_Length || IsListEmpty(MSG_Q(cnn)) ) {

            ReplyMsg((struct Message *)req);
            req = NULL;            
        }
    }

    if( br != NULL ) {
        AddHead(MSG_Q(cnn), br);
    }
}

VOID APB_SendToClient(struct Connection *cnn, struct BufferRef *br)
{
    AddTail(MSG_Q(cnn), TO_NODE(br));

    APB_ReplyToRead(cnn);
}

VOID APB_HandleReadRequest(struct Connection *cnn, struct Request *req)
{
    AddTail(REQ_Q(cnn), TO_NODE(req));

    APB_ReplyToRead(cnn);
}

