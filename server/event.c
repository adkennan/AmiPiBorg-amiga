
#include "server.h"

VOID Connect(struct Server *srv, struct Requst *msg)
{
    struct Connection *conn;
    struct ConnRequest cr

    if( conn = (struct Connection *)APB_AllocMem(sizeof(struct Connection), MEMF_ANY) ) {

        conn->c_Id
        conn->c_State = CS_CONNECTED;
        conn->c_HandlerId = msg->cr_HandlerID;
        

    }
}

VOID Disconnect(struct Server *srv, struct Request *msg)
{

}

VOID ReadData(struct Server *srv, struct Request *msg)
{

}

VOID WriteData(struct Server *srv, struct Request *msg)
{

}

VOID ProcessClientMessages(struct Server *srv)
{
    struct Request *msg;

    while( msg = (struct Request *)GetMsg(srv->s_ClientPort) ) {

        switch( msg->r_MsgType ) {

            case APB_M_CONNECT:
                Connect(srv, msg);
                break;

            case APB_M_DISCONNECT:
                Disconnect(srv, msg);
                break;
    
	        case APB_M_CHECK:
                ReplyMsg(msg);
                break;

            case APB_M_READ:
                ReadData(srv, msg);
                break;

            case APB_M_WRITE:
                WriteData(srv, msg);
                break;

	        case APB_M_ABORT:
                ReplyMsg(msg);
                break;
        }
    }
}

VOID APB_RunEventQueue(struct Server *srv)
{
    struct MsgPort *clientPort;
    struct Message *msg;
    BOOL running = TRUE;
    ULONG sig;

    while(running) {

        sig = Wait(SIGBREAKF_CTRL_C 
                   | (1 << srv->s_ClientPort->mp_SigBit)
                   | (1 << srv->s_ServerPort->mp_SigBit));

        if( sig & srv->s_ClientPort->mp_SigBit ) {
            ProcessClientMessages(srv);
        }

        if( sig & srv->s_ServerPort->mp_SigBit ) {
            ProcessServerMessages(srv); 
        }

        if( sig & SIGBREAKF_CTRL_C ) {
            running = FALSE;
        }
    }

    while( msg = GetMsg(srv->s_ClientPort) ) {
        ReplyMsg(msg);
    }

    while( msg = GetMsg(srv->s_ServerPort) ) {
        ReplyMsg(msg);
    }
}
