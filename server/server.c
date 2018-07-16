
#include "server.h"

#include "log.h"
#include "memory.h"
#include "config.h"
#include "objectpool.h"
#include "buffer.h"
#include "packetwriter.h"
#include "packetreader.h"
#include "connection.h"
#include "stats.h"
#include "remote.h"

#include "amipiborg.h"

#include "remote_protos.h"
#include "remote_pragmas.h"

#include <exec/io.h>
#include <exec/lists.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <devices/timer.h>

#include <stdio.h>

#define PING_TICKS 5
#define MAX_MISSED_PINGS 5

#define TICK_MILLIS 1000

enum ServerState {
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

struct Server {
    MemoryPool srv_MemPool;
    ObjectPool srv_ObjPool;
    PacketWriter srv_PacketWriter;
    PacketReader srv_PacketReader;
    Remote    srv_Remote;
    struct Config *srv_Config;
    struct Buffer *srv_ReadBuffer;
    struct MsgPort *srv_ClientPort;
    struct MsgPort *srv_TimerPort;
    struct timerequest *srv_TimerReq;
    struct MinList srv_Connections;
    UWORD     srv_State;
    UWORD     srv_NextConnId;
    UWORD     srv_LastInPackId;
    UWORD     srv_PingSendTicks;
    UWORD     srv_MissedPingCount;
    UWORD     srv_PingTimeoutTicks;
};

struct Library *RemoteBase;

#define CONNECTIONS(s) ((struct List *)&s->srv_Connections)

#define MODULE "Server"

#define ORDIE(msg) else { LOG0(LOG_ERROR, msg); }

BOOL APB_ConfigureRemote(
    struct Server *srv)
{
    struct RemoteArgs *ra = REM_GetArgTemplate(srv->srv_Remote);

    if(ra) {

        if(srv->srv_Config->cf_RemoteArgs) {
            if(!APB_ParseRemoteArgs(srv->srv_Config, ra)) {
                return FALSE;
            }
        }

        REM_ConfigureRemote(srv->srv_Remote, ra);
    }

    return TRUE;
}

Server APB_CreateServer(
    VOID)
{
    struct Server *srv;
    struct Config *cfg;
    MemoryPool mp;

    if(mp = APB_CreatePool()) {

        if(cfg = APB_GetConfig(mp)) {

            APB_InitLog(mp, cfg->cf_LogLevel, 1024, cfg->cf_LogToStdOut);

            LOG0(LOG_DEBUG, "Create Server");

            if(srv = APB_AllocMem(mp, sizeof(struct Server))) {

                srv->srv_MemPool = mp;
                srv->srv_Config = cfg;
                srv->srv_NextConnId = 1;
                srv->srv_ClientPort = NULL;
                srv->srv_TimerPort = NULL;
                srv->srv_TimerReq = NULL;
                srv->srv_State = SS_STARTING;
                srv->srv_MissedPingCount = 0;
                srv->srv_LastInPackId = 0;
                srv->srv_PingSendTicks = MAX_MISSED_PINGS;
                srv->srv_MissedPingCount = 0;
                srv->srv_PingTimeoutTicks = 0;
                srv->srv_ReadBuffer = NULL;

                NewList(CONNECTIONS(srv));

                LOG0(10, "Create Object Pool");

                if(srv->srv_ObjPool = APB_CreateObjectPool(srv->srv_MemPool)) {

                    LOG0(LOG_DEBUG, "Create Packet Writer");

                    if(srv->srv_PacketWriter = APB_CreatePacketWriter(srv->srv_MemPool, srv->srv_ObjPool)) {

                        LOG0(LOG_DEBUG, "Create Packet Reader");

                        if(srv->srv_PacketReader = APB_CreatePacketReader(srv->srv_MemPool, srv->srv_ObjPool)) {

                            LOG1(LOG_DEBUG, "Open Remote Library %s", cfg->cf_RemoteName);

                            if(RemoteBase = OpenLibrary(cfg->cf_RemoteName, 1)) {

                                LOG0(LOG_DEBUG, "Create Remote");

                                if(srv->srv_Remote = REM_CreateRemote()) {

                                    LOG0(LOG_DEBUG, "Configure Remote");
                                    if(APB_ConfigureRemote(srv)) {

                                        LOG0(LOG_DEBUG, "Create Timer Port");

                                        if(srv->srv_TimerPort = CreateMsgPort()) {

                                            LOG0(LOG_DEBUG, "Create Timer Request");

                                            if(srv->srv_TimerReq = (struct timerequest *)
                                               CreateExtIO(srv->srv_TimerPort, sizeof(struct timerequest))) {

                                                LOG0(LOG_DEBUG, "Open Timer Device");

                                                if(OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)
                                                              srv->srv_TimerReq, 0L) == 0) {

                                                    LOG0(LOG_DEBUG, "Server Created");

                                                    return srv;

                                                }
                                                ORDIE("Failed to open Timer Device")
                                                    DeleteExtIO((struct IORequest *) srv->srv_TimerReq);

                                            }
                                            ORDIE("Failed to create Timer Request")
                                                APB_DeletePort(srv->srv_TimerPort);

                                        }
                                    ORDIE("Failed to create Timer Port")}
                                    ORDIE("Failed to configure remote");

                                    REM_DestroyRemote(srv->srv_Remote);

                                }
                                ORDIE("Failed to create remote")
                                    CloseLibrary(RemoteBase);

                            }
                            ORDIE("Failed to open Remote Library")
                                APB_DestroyPacketReader(srv->srv_PacketReader);

                        }
                        ORDIE("Failed to create Packet Reader")
                            APB_DestroyPacketWriter(srv->srv_PacketWriter);

                    }
                    ORDIE("Failed to create Packet Writer")
                        APB_DestroyObjectPool(srv->srv_ObjPool);

                }
                ORDIE("Failed to create Object Pool")
                    APB_FreeMem(mp, srv, sizeof(struct Server));
            }

            APB_DestroyLog();

            APB_FreeConfig(cfg);
        }

        APB_DestroyPool(mp);
    }

    return NULL;
}

VOID APB_DeletePort(
    struct MsgPort * port)
{

    struct Message *msg;

    if(port) {
        while(msg = GetMsg(port)) {
            ReplyMsg(msg);
        }

        if(port->mp_Node.ln_Name) {
            DeletePort(port);
        } else {
            DeleteMsgPort(port);
        }
    }
}


VOID APB_DestroyServer(
    Server server)
{
    struct Server *srv = (struct Server *) server;
    Connection cnn;

    MemoryPool mp;

    if(srv->srv_TimerReq) {

        LOG0(LOG_DEBUG, "Close Timer");

        AbortIO((struct IORequest *) srv->srv_TimerReq);
        WaitIO((struct IORequest *) srv->srv_TimerReq);

        CloseDevice((struct IORequest *) srv->srv_TimerReq);
        DeleteExtIO((struct IORequest *) srv->srv_TimerReq);
    }

    if(srv->srv_TimerPort) {
        DeleteMsgPort(srv->srv_TimerPort);
    }

    while(!IsListEmpty(CONNECTIONS(srv))) {

        cnn = (Connection) RemTail(CONNECTIONS(srv));
        LOG1(LOG_INFO, "Destroy Connection %d", APB_GetId(cnn));
        APB_DestroyConnection(cnn);
    }

    if(RemoteBase) {

        LOG0(LOG_DEBUG, "Close Remote");

        if(srv->srv_Remote) {
            REM_DestroyRemote(srv->srv_Remote);
        }

        CloseLibrary(RemoteBase);
    }

    if(srv->srv_PacketReader) {
        LOG0(LOG_DEBUG, "Destroy Packet Reader");
        APB_DestroyPacketReader(srv->srv_PacketReader);
    }

    if(srv->srv_PacketWriter) {
        LOG0(LOG_DEBUG, "Destroy Packet Writer");
        APB_DestroyPacketWriter(srv->srv_PacketWriter);
    }

    if(srv->srv_ObjPool) {
        LOG0(LOG_DEBUG, "Destroy ObjectPool");
        APB_DestroyObjectPool(srv->srv_ObjPool);
    }

    LOG0(LOG_DEBUG, "Destroy Server");

    mp = srv->srv_MemPool;

    APB_FreeConfig(srv->srv_Config);

    APB_FreeMem(mp, srv, sizeof(struct Server));

    LOG0(LOG_DEBUG, "Goodbye Cruel World");

    APB_DestroyLog();

    APB_DestroyPool(mp);
}


VOID APB_EnableTimer(
    struct Server *srv,
    ULONG timeoutMillis)
{
    LOG1(LOG_TRACE, "Set timer to %dms", timeoutMillis);

    AbortIO((struct IORequest *) srv->srv_TimerReq);

    srv->srv_TimerReq->tr_node.io_Command = TR_ADDREQUEST;
    srv->srv_TimerReq->tr_time.tv_secs = timeoutMillis / 1000;
    srv->srv_TimerReq->tr_time.tv_micro = (timeoutMillis % 1000) * 1000;

    SendIO((struct IORequest *) srv->srv_TimerReq);
}

VOID APB_SrvSendToRemote(
    struct Server *srv)
{
    if(!REM_CanWrite(srv->srv_Remote)) {
        return;
    }

    LOG0(LOG_TRACE, "Write Buffer");

    APB_WriteBuffer(srv->srv_PacketWriter, srv->srv_Remote);
}

VOID APB_SendPing(
    struct Server * srv)
{
    struct Packet *p;

    if(!(p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet)))) {
        return;
    }

    LOG0(LOG_TRACE, "Ping");

    p->pac_Type = PT_PING;
    p->pac_ConnId = DEFAULT_CONNECTION;

    APB_SrvSendToRemote(srv);

    srv->srv_PingTimeoutTicks = PING_TICKS;
    srv->srv_PingSendTicks = 0;
}

VOID APB_SendError(
    struct Server * srv,
    UWORD connId,
    UWORD errorCode)
{
    struct Packet *p;

    if(!(p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet)))) {
        return;
    }

    LOG2(LOG_ERROR, "Error: connId = %d, errorCode = %d", connId, errorCode);

    p->pac_Type = errorCode;
    p->pac_ConnId = connId;

    APB_SrvSendToRemote(srv);
}

VOID APB_HandleResendRequest(
    struct Server * srv,
    struct InPacket * ip)
{
    UWORD     packId = *((UWORD *) ip->ip_Data1);

    LOG1(LOG_INFO, "Resend request for packet %d", packId);

    APB_ResendPacket(srv->srv_PacketWriter, packId);
}

VOID APB_ConfigureServer(
    struct Server *srv,
    struct InPacket *ip)
{
    struct PacketHello *p;
    struct HandlerDesc *h;
    UWORD     size = ip->ip_Data1Length + ip->ip_Data2Length;
    UWORD     ix;

    if(!(p = APB_AllocMem(srv->srv_MemPool, size))) {
        LOG1(LOG_ERROR, "Can't allocate %d bytes for hello packet", size);
        return;
    }

    APB_InPacketToBuffer(ip, p);

    LOG1(LOG_INFO, "Server version %d", p->ph_Version);

    LOG1(LOG_INFO, "%d Handlers Available:", p->ph_HandlerCount);
    if(p->ph_HandlerCount > 0) {
        h = (struct HandlerDesc *) (p + 1);

        for(ix = 0; ix < p->ph_HandlerCount; ix++) {

            LOG2(LOG_INFO, "\t%d: %s", h->hd_HandlerId, h->hd_HandlerName);

            h++;
        }
    }

    APB_FreeMem(srv->srv_MemPool, p, size);
}

VOID APB_HandleControlPacket(
    struct Server *srv,
    struct InPacket *ip)
{
    switch (ip->ip_Type) {

    case PT_HELLO:
        LOG0(LOG_DEBUG, "Hello Received");
        srv->srv_State = SS_CONNECTED;
        APB_ConfigureServer(srv, ip);
        APB_SendPing(srv);
        break;

    case PT_GOODBYE:
        LOG0(LOG_DEBUG, "Goodbye Received");
        srv->srv_State = SS_DISCONNECTED;
        break;

    case PT_PONG:
        LOG0(LOG_TRACE, "Ping Received");
        srv->srv_MissedPingCount = 0;
        break;

    case PT_RESEND:
        LOG0(LOG_DEBUG, "Resend Request Received");
        APB_HandleResendRequest(srv, ip);
        break;

    default:
        LOG1(LOG_ERROR, "Unknown control packet %d", ip->ip_Type);
        break;
    }

    APB_ReleaseInPacket(ip);
}

VOID APB_RequestResend(
    struct Server *srv,
    UWORD currPackId)
{
    struct PacketResend *p;
    UWORD     packId = srv->srv_LastInPackId + 1;

    while(packId < currPackId) {

        if(!(p = (struct PacketResend *) APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct PacketResend)))) {
            break;
        }

        LOG1(LOG_DEBUG, "Requesting Resend of packet %d", packId);

        p->pr_Pac.pac_Type = PT_RESEND;
        p->pr_Pac.pac_ConnId = DEFAULT_CONNECTION;
        p->pr_PacketId = packId;
        packId++;
    }
}


VOID APB_HandlePacket(
    struct Server *srv,
    struct InPacket *ip)
{
    Connection cnn;

    LOG1(LOG_TRACE, "Received packet %d", ip->ip_PackId);

    srv->srv_PingSendTicks = PING_TICKS;
    srv->srv_PingTimeoutTicks = 0;

    if(!(ip->ip_Flags & PF_RESEND)) {
        if(ip->ip_PackId - 1 > srv->srv_LastInPackId) {
            LOG2(LOG_DEBUG, "Expecting packet %d, got %d", srv->srv_LastInPackId + 1, ip->ip_PackId);

            APB_RequestResend(srv, ip->ip_PackId);
        }

        srv->srv_LastInPackId = ip->ip_PackId;
    } else {
        LOG1(LOG_DEBUG, "Received resent packet %d", ip->ip_PackId);
    }

    if(ip->ip_ConnId == DEFAULT_CONNECTION) {
        APB_HandleControlPacket(srv, ip);

    } else {

        cnn = APB_FindConnection(CONNECTIONS(srv), ip->ip_ConnId);
        if(cnn == NULL) {

            LOG1(LOG_DEBUG, "Packet received for unknown connection %d", ip->ip_ConnId);

            APB_SendError(srv, ip->ip_ConnId, PT_NO_CONNECTION);
            APB_ReleaseInPacket(ip);

        } else {

            APB_HandleConnectionPacket(cnn, ip);
        }
    }
}

VOID APB_SrvBeginRead(
    struct Server *srv)
{
    struct Buffer *buf;

    if(srv->srv_ReadBuffer == NULL || BUF_FULL(srv->srv_ReadBuffer)) {
        if(!(buf = APB_AllocateBuffer(srv->srv_ObjPool))) {
            LOG0(LOG_ERROR, "Unable to allocate read buffer");
        } else {
            srv->srv_ReadBuffer = buf;
        }
    }

    if(!REM_CanRead(srv->srv_Remote)) {
        return;
    }

    LOG1(LOG_TRACE, "Read up to %d bytes", BUF_AVAIL(srv->srv_ReadBuffer));

    REM_Read(srv->srv_Remote, BUF_CURR(srv->srv_ReadBuffer), BUF_AVAIL(srv->srv_ReadBuffer));
}

VOID APB_SrvReceiveFromRemote(
    struct Server * srv)
{
    struct InPacket *ip;
    UWORD     bytesRead;

    if(!REM_CanRead(srv->srv_Remote)) {

        return;
    }

    bytesRead = REM_BytesRead(srv->srv_Remote);
    if(bytesRead == 0) {
        return;
    }

    LOG1(LOG_DEBUG, "Read %d bytes", bytesRead);
    if(APB_ShouldLog(LOG_TRACE)) {
        APB_LogMem(__FILE__, __LINE__, __FUNC__, LOG_TRACE, BUF_CURR(srv->srv_ReadBuffer), bytesRead);
    }

    srv->srv_ReadBuffer->b_Offset += bytesRead;

    APB_ProcessBuffer(srv->srv_PacketReader, srv->srv_ReadBuffer);

    APB_SrvBeginRead(srv);

    while(APB_ReaderQueueSize(srv->srv_PacketReader) > 0) {

        ip = APB_DequeueInPacket(srv->srv_PacketReader);

        APB_HandlePacket(srv, ip);
    }
}

VOID APB_ReceiveFromClient(
    struct Server *srv)
{
    struct APBRequest *req;
    Connection cnn;

    while(req = (struct APBRequest *) GetMsg(srv->srv_ClientPort)) {

        if(req->r_Type == APB_RT_OPEN) {

            cnn = APB_CreateConnection(CONNECTIONS(srv), srv->srv_ObjPool, srv->srv_NextConnId++, srv->srv_PacketWriter);
        } else {

            cnn = APB_FindConnection(CONNECTIONS(srv), req->r_ConnId);
        }

        if(cnn == NULL) {

            LOG1(LOG_DEBUG, "Received message from invalid connection %d", req->r_ConnId);

            req->r_State = APB_RS_NO_CONNECTION;
            ReplyMsg((struct Message *) req);
        } else {

            APB_HandleClientRequest(cnn, req);
        }
    }
}

VOID APB_HandleTimer(
    struct Server *srv)
{
    WaitIO((struct IORequest *) srv->srv_TimerReq);

    switch (srv->srv_State) {

    case SS_WAIT_FOR_CONNECT:
        srv->srv_State = SS_CONNECT_FAILED;
        break;

    case SS_CONNECTED:
        if(srv->srv_PingTimeoutTicks == 1) {
            srv->srv_PingTimeoutTicks = 0;
            srv->srv_MissedPingCount++;

            if(srv->srv_MissedPingCount > MAX_MISSED_PINGS) {
                srv->srv_State = SS_CONNECTION_LOST;
            }
        } else if(srv->srv_PingTimeoutTicks > 1) {
            srv->srv_PingTimeoutTicks--;
        }

        if(srv->srv_PingSendTicks == 1) {
            APB_SendPing(srv);

        } else if(srv->srv_PingSendTicks > 1) {
            srv->srv_PingSendTicks--;
        }

        APB_CheckRequestTimeouts(CONNECTIONS(srv));

        break;

    case SS_DISCONNECTING:
        srv->srv_State = SS_DISCONNECTED;
        break;
    }

    if(srv->srv_State != SS_DISCONNECTED) {

        APB_EnableTimer(srv, TICK_MILLIS);
    }
}

VOID APB_WaitForMessage(
    struct Server *srv)
{
    ULONG     sig, remoteSigBits;

    remoteSigBits = REM_GetSignalBits(srv->srv_Remote);

    sig = SIGBREAKF_CTRL_C | remoteSigBits | (srv->srv_ClientPort ? (1 << srv->srv_ClientPort->mp_SigBit) : 0)
        | (1 << srv->srv_TimerPort->mp_SigBit);

    LOG1(LOG_TRACE, "Wait for sigs 0x%x", sig);

    sig = Wait(sig);

    LOG1(LOG_TRACE, "Received sig 0x%x", sig);

    if(sig & SIGBREAKF_CTRL_C) {
        LOG0(LOG_INFO, "Break!");
        srv->srv_State = SS_DISCONNECTED;
    }

    if(sig & (1 << srv->srv_TimerPort->mp_SigBit)) {
        APB_HandleTimer(srv);
    }

    if(srv->srv_ClientPort && (sig & (1 << srv->srv_ClientPort->mp_SigBit))) {
        APB_ReceiveFromClient(srv);
    }

    if(sig & remoteSigBits) {
        REM_HandleSignal(srv->srv_Remote, sig);
    }

    APB_SrvReceiveFromRemote(srv);
    APB_SrvSendToRemote(srv);
}

BOOL APB_OpenClient(
    struct Server *srv)
{
    LOG0(LOG_DEBUG, "Open Client Port");

    if(srv->srv_ClientPort = CreatePort(PORT_NAME, 0)) {

        return TRUE;
    }

    return FALSE;
}

BOOL APB_SendInit(
    struct Server * srv)
{
    struct PacketInit *p;

    if(!(p = (struct PacketInit *) APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct PacketInit)))) {
        return FALSE;
    }

    LOG0(LOG_DEBUG, "Send Init");
    LOG1(LOG_DEBUG, "Version = %d", AMIPIBORG_VERSION);
    LOG1(LOG_DEBUG, "MaxPacketSize = %d", BUFFER_SIZE);

    srv->srv_State = SS_WAIT_FOR_CONNECT;

    p->pi_Pac.pac_Type = PT_INIT;
    p->pi_Pac.pac_ConnId = DEFAULT_CONNECTION;
    p->pi_Version = AMIPIBORG_VERSION;
    p->pi_MaxPacketSize = BUFFER_SIZE;

    APB_SrvSendToRemote(srv);

    return TRUE;
}

BOOL APB_SendShutdown(
    struct Server * srv)
{
    struct Packet *p;

    if(!(p = APB_AllocPacket(srv->srv_PacketWriter, sizeof(struct Packet)))) {
        return FALSE;
    }

    LOG0(LOG_DEBUG, "Send Shutdown");

    p->pac_Type = PT_SHUTDOWN;
    p->pac_ConnId = DEFAULT_CONNECTION;

    APB_SrvSendToRemote(srv);

    return TRUE;
}


VOID APB_Run(
    Server server)
{
    struct Server *srv = (struct Server *) server;
    UWORD     retryCount = 5;
    BOOL      remoteOpen, clientOpen;

    srv->srv_State = SS_STARTING;

    APB_EnableTimer(srv, TICK_MILLIS);

    while(srv->srv_State != SS_STOPPED) {

        switch (srv->srv_State) {

        case SS_STARTING:
            LOG0(LOG_TRACE, "Starting");

            retryCount = 5;

            srv->srv_State = SS_OPENING_REMOTE;
            break;

        case SS_OPENING_REMOTE:
            LOG0(LOG_TRACE, "Opening Remote");

            if(remoteOpen = REM_OpenRemote(srv->srv_Remote)) {
                APB_SrvBeginRead(srv);
                srv->srv_State = SS_OPENING_CLIENT;
            } else {
                srv->srv_State = SS_STOPPING;
            }
            break;

        case SS_OPENING_CLIENT:
            LOG0(LOG_TRACE, "Opening Client");

            if(clientOpen = APB_OpenClient(srv)) {
                srv->srv_State = SS_CONNECTING;
            } else {
                srv->srv_State = SS_STOPPING;
            }
            break;


        case SS_CONNECTING:
            LOG0(LOG_TRACE, "Connecting");

            if(APB_SendInit(srv)) {
                APB_WaitForMessage(srv);
            } else {
                srv->srv_State = SS_STOPPING;
            }
            break;

        case SS_WAIT_FOR_CONNECT:
            LOG0(LOG_TRACE, "Wait For Connect");

            APB_WaitForMessage(srv);
            break;

        case SS_CONNECT_FAILED:
            LOG0(LOG_TRACE, "Connect Failed");

            retryCount--;
            if(retryCount == 0) {
                srv->srv_State = SS_STOPPING;
            } else {
                srv->srv_State = SS_CONNECTING;
            }
            break;


        case SS_CONNECTED:
            APB_WaitForMessage(srv);
            break;

        case SS_CONNECTION_LOST:
            LOG0(LOG_TRACE, "Connection Lost");

            srv->srv_State = SS_CONNECTING;
            break;

        case SS_DISCONNECTING:
            LOG0(LOG_TRACE, "Disconnecting");

            if(APB_SendShutdown(srv)) {
                APB_WaitForMessage(srv);
            } else {
                srv->srv_State = SS_STOPPING;
            }
            break;

        case SS_DISCONNECTED:
            LOG0(LOG_TRACE, "Disconnected");

            srv->srv_State = SS_STOPPING;
            break;

        case SS_STOPPING:
            LOG0(LOG_TRACE, "Stopping");

            srv->srv_State = SS_CLOSING_CLIENT;
            break;

        case SS_CLOSING_CLIENT:
            LOG0(LOG_TRACE, "Closing Client");

            APB_DeletePort(srv->srv_ClientPort);
            srv->srv_ClientPort = NULL;

            srv->srv_State = SS_CLOSING_REMOTE;
            break;

        case SS_CLOSING_REMOTE:
            LOG0(LOG_TRACE, "Closing Remote");

            REM_CloseRemote(srv->srv_Remote);
            srv->srv_State = SS_STOPPED;
            break;
        }
    }
    LOG0(LOG_TRACE, "Stopped");
}
