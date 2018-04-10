
#include "server.h"

#include "memory.h"
#include "objectpool.h"
#include "buffer.h"
#include "packetwriter.h"
#include "packetreader.h"
#include "remote.h"
#include "connection.h"
#include "stats.h"

#include "amipiborg.h"

#include <exec/io.h>
#include <exec/lists.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <devices/timer.h>

#include <stdio.h>

#define MAX_MISSED_PINGS 5

enum ServerState
{
    SS_STARTING,
    SS_OPENING_REMOTE,
    SS_OPENING_CLIENT,
    SS_CONNECTING,
    SS_WAIT_FOR_CONNECT,
    SS_CONNECTED,
    SS_CONNECTION_LOST,
    SS_CONNECT_FAILED,
    SS_DISCONNECTING,
    SS_WAIT_FOR_DISCONNECT,
    SS_DISCONNECTED,
    SS_STOPPING,
    SS_CLOSING_CLIENT,
    SS_CLOSING_REMOTE,
    SS_STOPPED
};

struct Server 
{
    MemoryPool          srv_MemPool;
    ObjectPool          srv_ObjPool;
    PacketWriter        srv_PacketWriter;
    PacketReader        srv_PacketReader;
    Remote              srv_Remote;
    struct MsgPort     *srv_ClientPort;
    struct MsgPort     *srv_TimerPort;
    struct timerequest *srv_TimerReq;
    struct MinList      srv_Connections;
    UWORD               srv_State;
    UWORD               srv_NextConnId;
    UWORD               srv_MissedPingCount;
    UWORD               srv_LastInPackId;
    BOOL                srv_PingPending;
};

#define CONNECTIONS(s) ((struct List *)&s->srv_Connections)

Server APB_CreateServer(VOID)
{
    struct Server *srv;
    MemoryPool mp;

    if( mp = APB_CreatePool() ) {

        if( srv = APB_AllocMem(mp, sizeof(struct Server) ) ) {

            srv->srv_MemPool = mp;
            srv->srv_NextConnId = 1;
            srv->srv_ClientPort = NULL;
            srv->srv_TimerPort = NULL;
            srv->srv_TimerReq = NULL;
            srv->srv_State = SS_STARTING;
            srv->srv_MissedPingCount = 0;
            srv->srv_LastInPackId = 0;
            srv->srv_PingPending = FALSE;
            
            NewList(CONNECTIONS(srv));

            if( srv->srv_ObjPool = APB_CreateObjectPool(srv->srv_MemPool) ) {

                if( srv->srv_PacketWriter = APB_CreatePacketWriter(srv->srv_MemPool, srv->srv_ObjPool) ) {

                    if( srv->srv_PacketReader = APB_CreatePacketReader(srv->srv_MemPool, srv->srv_ObjPool) ) {
    
                        if( srv->srv_Remote = APB_CreateRemote(srv->srv_MemPool, srv->srv_ObjPool) ) {

                            if( srv->srv_TimerPort = CreateMsgPort() ) {
    
                                if( srv->srv_TimerReq =  (struct timerequest *)CreateExtIO(srv->srv_TimerPort, sizeof(struct timerequest) ) ) {

                                    if( OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)srv->srv_TimerReq, 0L) == 0 ) {
                                
                                        return srv;

                                    }
                
                                    DeleteExtIO((struct IORequest *)srv->srv_TimerReq);
                                }

                                APB_DeletePort(srv->srv_TimerPort);
                            }

                            APB_DestroyRemote(srv->srv_Remote);
                        }
    
                        APB_DestroyPacketReader(srv->srv_PacketReader);
                    }

                    APB_DestroyPacketWriter(srv->srv_PacketWriter);
                }

                APB_DestroyObjectPool(srv->srv_ObjPool);
            }

            APB_FreeMem(mp, srv, sizeof(struct Server));
        }
        
        APB_DestroyPool(mp);
    }

    return NULL;
}

VOID APB_DeletePort(struct MsgPort *port) {

    struct Message *msg;

    if( port ) {
        while( msg = GetMsg(port) ) {
            ReplyMsg(msg);
        }

        if( port->mp_Node.ln_Name ) {
            DeletePort(port);
        } else {
            DeleteMsgPort(port);
        }
    }
}


VOID APB_DestroyServer(Server server)
{
    struct Server *srv = (struct Server *)server;

    MemoryPool mp;

    if( srv->srv_TimerReq ) {
        AbortIO((struct IORequest *)srv->srv_TimerReq);
        WaitIO((struct IORequest *)srv->srv_TimerReq);

        CloseDevice((struct IORequest *)srv->srv_TimerReq);
        DeleteExtIO((struct IORequest *)srv->srv_TimerReq);
    }

    while( !IsListEmpty(CONNECTIONS(srv)) ) {

        APB_DestroyConnection((Connection)RemTail(CONNECTIONS(srv)));
    }

    if( srv->srv_TimerPort ) {
        DeleteMsgPort(srv->srv_TimerPort);
    }

    APB_DestroyRemote(srv->srv_Remote);

    if( srv->srv_PacketReader ) {
        APB_DestroyPacketReader(srv->srv_PacketReader);
    }

    if( srv->srv_PacketWriter ) {
        APB_DestroyPacketWriter(srv->srv_PacketWriter);
    }

    if( srv->srv_ObjPool ) {
        APB_DestroyObjectPool(srv->srv_ObjPool);
    }

    mp = srv->srv_MemPool;
    APB_FreeMem(mp, srv, sizeof(struct Server));

    APB_DestroyPool(mp);    
}


VOID APB_EnableTimer(struct Server *srv,ULONG timeoutMillis)
{
    AbortIO((struct IORequest *)srv->srv_TimerReq);

    srv->srv_TimerReq->tr_node.io_Command = TR_ADDREQUEST;
    srv->srv_TimerReq->tr_time.tv_secs = timeoutMillis / 1000;
    srv->srv_TimerReq->tr_time.tv_micro = (timeoutMillis % 1000) * 1000;

    SendIO((struct IORequest *)srv->srv_TimerReq);
}

VOID APB_SrvSendToRemote(struct Server *srv)
{
    struct Buffer *buf;

    if( APB_WriterQueueSize(srv->srv_PacketWriter) == 0 ) {
        return;
    }

    if( !APB_CanSendToRemote(srv->srv_Remote) ) {
        return;
    }

    buf = APB_DequeueBuffer(srv->srv_PacketWriter);

    APB_IncrementStat(ST_BYTES_SENT, buf->b_Offset);            

    APB_SendToRemote(srv->srv_Remote, buf);
}

VOID APB_SendPing(struct Server *srv)
{
    struct Packet *p;

    if( ! (p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet) ) ) ) {
        return;
    }

    //printf("Ping...\n");

    p->pac_Type      = PT_PING;
    p->pac_ConnId    = DEFAULT_CONNECTION;

    APB_SrvSendToRemote(srv);

    srv->srv_PingPending = TRUE;

    APB_EnableTimer(srv, 5000);
}

VOID APB_SendError(struct Server *srv, UWORD connId, UWORD errorCode)
{
    struct Packet *p;

    if( ! (p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet) ) ) ) {
        return;
    }

    p->pac_Type      = errorCode;
    p->pac_ConnId    = connId;

    APB_SrvSendToRemote(srv);
}

VOID APB_HandleControlPacket(struct Server *srv, struct PacketRef *pr)
{

    switch( pr->pr_Type ) {
    
        case PT_HELLO:
            srv->srv_State = SS_CONNECTED;
            // TODO: Get list of handlers.

            APB_SendPing(srv);
            break;
    
        case PT_GOODBYE:
            srv->srv_State = SS_DISCONNECTED;
            break;

        case PT_PONG:
            srv->srv_MissedPingCount = 0;
            srv->srv_PingPending = FALSE;           
            //printf("Pong...\n");
            break;
    }
    
    APB_ReleasePacketRef(pr);
}

VOID APB_RequestResend(struct Server *srv, UWORD currPackId)
{
    struct PacketResend *p;
    UWORD packId = srv->srv_LastInPackId;

    while( packId < currPackId ) {
    
        if( ! (p = (struct PacketResend *)APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct PacketResend) ) ) ) {
            break;
        }

        p->pr_Pac.pac_Type      = PT_RESEND;
        p->pr_Pac.pac_ConnId    = DEFAULT_CONNECTION;
        p->pr_PacketId = packId;
        packId++;
    }
}


VOID APB_HandlePacket(struct Server *srv, struct PacketRef *pr)
{
    Connection cnn;
	UWORD ix;

    if( !(pr->pr_Flags & PF_RESEND) ) {
		if( pr->pr_PackId - 1 > srv->srv_LastInPackId ) {
        	printf("Expecting packet %d, got %d\n", srv->srv_LastInPackId + 1, pr->pr_PackId);
			
			for( ix = srv->srv_LastInPackId + 1; ix < pr->pr_PackId; ix++ ) {
			    APB_RequestResend(srv, ix);
			}
    	}
		
    	srv->srv_LastInPackId = pr->pr_PackId;
	} else {
		printf("Received resent packet %d\n", pr->pr_PackId);
	}

    if( pr->pr_ConnId == DEFAULT_CONNECTION ) {
        APB_HandleControlPacket(srv, pr); 

    } else {

        cnn = APB_FindConnection(CONNECTIONS(srv), pr->pr_ConnId);
        if( cnn == NULL ) {       

            APB_SendError(srv, pr->pr_ConnId, PT_NO_CONNECTION);
            APB_ReleasePacketRef(pr);

        } else {

            APB_HandleConnectionPacket(cnn, pr);
        }
    }
}

VOID APB_SrvReceiveFromRemote(struct Server *srv)
{
    struct Buffer *buf;
    struct PacketRef *pr;

    if( !APB_CanReceiveFromRemote(srv->srv_Remote) ) {
    
        return;
    }

    buf = APB_ReceiveFromRemote(srv->srv_Remote);

    APB_ProcessBuffer(srv->srv_PacketReader, buf);

    while( APB_ReaderQueueSize(srv->srv_PacketReader) > 0 ) {

        pr = APB_DequeuePacketRef(srv->srv_PacketReader);

        APB_HandlePacket(srv, pr);
    }    
}

VOID APB_ReceiveFromClient(struct Server *srv)
{
    struct APBRequest *req;
    Connection cnn;
    while( req = (struct APBRequest *)GetMsg(srv->srv_ClientPort ) ) {
    
        if( req->r_Type == APB_RT_OPEN ) {

            cnn = APB_CreateConnection(CONNECTIONS(srv), srv->srv_ObjPool, srv->srv_NextConnId++, srv->srv_PacketWriter);
        } else {

            cnn = APB_FindConnection(CONNECTIONS(srv), req->r_ConnId);
        }

        if( cnn == NULL ) {

            req->r_State = APB_RS_NO_CONNECTION;
            ReplyMsg((struct Message *)req);
        } else {
    
            APB_HandleClientRequest(cnn, req);
        }
    }
}

VOID APB_HandleTimer(struct Server *srv)
{
    WaitIO((struct IORequest *)srv->srv_TimerReq);

    switch(srv->srv_State ) {       

        case SS_WAIT_FOR_CONNECT:        
            srv->srv_State = SS_CONNECT_FAILED;
            break;

        case SS_CONNECTED:
            if( srv->srv_PingPending ) {
                srv->srv_MissedPingCount++;
            }

            if( srv->srv_MissedPingCount > MAX_MISSED_PINGS ) {
                printf("Connection Lost\n");
                srv->srv_State = SS_CONNECTION_LOST;

            } else {
                        
                APB_SendPing(srv);                    
            }                  
            break;

        case SS_DISCONNECTING:
            srv->srv_State = SS_DISCONNECTED;
            break;
    }
}

VOID APB_WaitForMessage(struct Server *srv)
{
    ULONG sig, remoteSigBits;

    remoteSigBits = APB_GetSigBits(srv->srv_Remote);

    sig = Wait(SIGBREAKF_CTRL_C 
               | remoteSigBits
               | (srv->srv_ClientPort ? (1 << srv->srv_ClientPort->mp_SigBit) : 0)
               | (1 << srv->srv_TimerPort->mp_SigBit));

    if( sig & SIGBREAKF_CTRL_C ) {
        printf("Break!\n");
        srv->srv_State = SS_DISCONNECTED;        
    }

    if( sig & (1 << srv->srv_TimerPort->mp_SigBit ) ) {
        //printf("Tick!\n");
        APB_HandleTimer(srv);    
    }

    if( srv->srv_ClientPort && (sig & (1 << srv->srv_ClientPort->mp_SigBit ) ) ) {
        //printf("Client!\n");
        APB_ReceiveFromClient(srv);
    }

    if( sig & remoteSigBits ) {
    
        //printf("Remote!\n");
        APB_HandleSignal(srv->srv_Remote, sig);
    }

    APB_SrvReceiveFromRemote(srv);
    APB_SrvSendToRemote(srv);
}

BOOL APB_OpenClient(struct Server *srv)
{
    if( srv->srv_ClientPort = CreatePort(PORT_NAME, 0) ) {

        return TRUE;
    }

    return FALSE; 
}

BOOL APB_SendInit(struct Server *srv)
{
    struct PacketInit *p;

    if( ! (p = (struct PacketInit *)APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct PacketInit) ) ) ) {
        return FALSE;
    }

    //printf("APB_SendInit\n");

    srv->srv_State = SS_WAIT_FOR_CONNECT;

    p->pi_Pac.pac_Type      = PT_INIT;
    p->pi_Pac.pac_ConnId    = DEFAULT_CONNECTION;
    p->pi_Version           = AMIPIBORG_VERSION;
    p->pi_MaxPacketSize     = BUFFER_SIZE;

    APB_SrvSendToRemote(srv);

    APB_EnableTimer(srv, 5000);
    
    return TRUE;
}

BOOL APB_SendShutdown(struct Server *srv)
{
    struct Packet *p;

    if( ! (p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet) ) ) ) {
        return FALSE;
    }

    p->pac_Type      = PT_SHUTDOWN;
    p->pac_ConnId    = DEFAULT_CONNECTION;

    APB_SrvSendToRemote(srv);

    APB_EnableTimer(srv, 2000);

    return TRUE;
}


VOID APB_Run(Server server)
{
    struct Server *srv = (struct Server *)server;
    UWORD retryCount = 5;
    BOOL remoteOpen, clientOpen;

    srv->srv_State = SS_STARTING;


    while(srv->srv_State != SS_STOPPED) {

        switch(srv->srv_State ) {

            case SS_STARTING:

                retryCount = 5;

                srv->srv_State = SS_OPENING_REMOTE;
                break;

            case SS_OPENING_REMOTE:

                if( remoteOpen = APB_OpenRemote(srv->srv_Remote) ) {
                    srv->srv_State = SS_OPENING_CLIENT;
                } else {
                    srv->srv_State = SS_STOPPING;
                }
                break;

            case SS_OPENING_CLIENT:

                if( clientOpen = APB_OpenClient(srv) ) {
                    srv->srv_State = SS_CONNECTING;
                } else {
                    srv->srv_State = SS_STOPPING;
                }
                break;
                

            case SS_CONNECTING:

                if( APB_SendInit(srv) ) {
                    APB_WaitForMessage(srv);
                } else {
                    srv->srv_State = SS_STOPPING;
                }
                break;

            case SS_WAIT_FOR_CONNECT:

                APB_WaitForMessage(srv);
                break;

            case SS_CONNECT_FAILED:
                retryCount--;
                if( retryCount == 0 ) {
                    srv->srv_State = SS_STOPPING;  
                } else {
                    srv->srv_State = SS_CONNECTING;
                }
                break;


            case SS_CONNECTED:

                APB_WaitForMessage(srv);
                break;

            case SS_CONNECTION_LOST:
                srv->srv_State = SS_CONNECTING;
                break;

            case SS_DISCONNECTING:

                if( APB_SendShutdown(srv) ) {
                    APB_WaitForMessage(srv);
                } else {
                    srv->srv_State = SS_STOPPING;
                }
                break;

            case SS_DISCONNECTED:

                srv->srv_State = SS_STOPPING;
                break;

            case SS_STOPPING:

                srv->srv_State = SS_CLOSING_CLIENT;
                break;

            case SS_CLOSING_CLIENT:

                APB_DeletePort(srv->srv_ClientPort);
                srv->srv_ClientPort = NULL;

                srv->srv_State = SS_CLOSING_REMOTE;
                break;

            case SS_CLOSING_REMOTE:

                APB_CloseRemote(srv->srv_Remote);
                srv->srv_State = SS_STOPPED;
                break;
        }
    }
}
