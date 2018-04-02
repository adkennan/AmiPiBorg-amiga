
#include "server.h"

VOID APB_HandleHello(struct Server *srv, struct Packet *pkt)
{
    struct PacketHello *ph;
    struct HandlerDesc *hd;
    struct HandlerNode *hn;
    UWORD i, len;
    STRPTR name;

    if( srv->s_RemoteVersion ) {
        // Already connected?
        return;
    }

    ph = (struct PacketHello *)pkt;

    srv->s_RemoteVersion = ph->ph_Version;

    hd = (struct HandlerDesc *)(p + sizeof(struct PacketHello));

    NewList(srv->s_Handlers);

    for( i = 0; i < ph->ph_HandlerCount; i++ ) {

        len = strlen(hd->hd_HandlerName);

        if( hn = (struct HandlerNode *)APB_AllocMem(sizeof(struct HandlerNode) + len + 1 ) ) {

            name = (STRPTR)hn + sizeof(struct HandlerNode);

            hn->hn_Node->n_Name = name;

            hn->Id = hd->hd_HandlerId;

            CopyMem(hd->hd_HandlerName, name, len);

            AddTail(srv->s_Handlers, (struct Node *)hn);
        }
    }
}

VOID APB_HandleGoodbye(struct Server *srv, struct Packet *pkt)
{
    struct HandlerNode *hn;
    UWORD len;

    if( ! srv->s_RemoteVersion ) {
        // Already disconnected.
        return;
    }

    while( ! IsListEmpty(srv->s_Handlers) ) {
    
        hn = RemTail(srv->s_Handlers);

        len = sizeof(struct HandlerNode) + strlen(hn->hn_Node->n_Name + 1;

        APB_FreeMem(hn, len);
    }
}

VOID APB_HandleConnected(struct Server *srv, struct Packet *pkt)
{
    struct Connection *cnn;
    
    if( cnn = FindConnection(pkt->pac_ConnId) ) {

        cnn->c_State = CS_CONNECTED;

    }
}

VOID APB_HandleDisconnected(struct Server *srv, struct Packet *pkt)
{
    struct Connection *cnn;

    if( cnn = FindConnection(pkt->pac_ConnId) ) {

        cnn->c_State = CS_DISCONNECTED;
    }
}


