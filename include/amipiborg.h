
#ifndef __AMIPIBORG_H__
#define __AMIPIBORG_H__

#include <exec/types.h>
#include <exec/ports.h>

#define PORT_NAME "AmiPiBorg"

#define APB_LibName "amipiborg.library"
#define APB_LibVersion 1
#define APB_LibRevision 0
#define APB_VersionStr "1.0 (18/03/2017)"

#define APB_PointerAdd(ptr, size) ((APTR)(((BYTE *)ptr) + size))

enum RequestState {
    APB_RS_OK = 1,
    APB_RS_FAILED,
    APB_RS_NOT_CONNECTED,
    APB_RS_NO_HANDLER,
    APB_RS_NO_CONNECTION,
    APB_RS_NO_SERVER,
    APB_RS_ABORTED,
    APB_RS_BUFFER_TOO_SMALL,
    APB_RS_TIMEOUT
};

enum RequestType {
    APB_RT_OPEN = 1,
    APB_RT_CLOSE,
    APB_RT_READ,
    APB_RT_WRITE,

    APB_RT_LOG = 50,

    APB_RT_SRV_QUIT = 100,
    APB_RT_SRV_STATS = 101,
    APB_RT_SRV_LOG_LEVEL = 102,
    APB_RT_SRV_LOG = 103,
    APB_RT_SRV_CONNS = 104
};

struct APBRequest {
    struct Message r_Msg;
    UWORD     r_Type;
    UWORD     r_ConnId;
    UWORD     r_HandlerId;
    UWORD     r_State;
    UWORD     r_Actual;
    UWORD     r_Length;
    UWORD     r_Timeout;
    UWORD     r_DataType;
    APTR      r_Data;
};

extern struct Library *AmiPiBorgBase;

#define LOG_ERROR 0
#define LOG_INFO 5
#define LOG_DEBUG 10
#define LOG_TRACE 20

#ifdef APB_NOLOG

#define LOG0(ctx,level,fmt)
#define LOG1(ctx,level,fmt,a)
#define LOG2(ctx,level,fmt,a,b)
#define LOG3(ctx,level,fmt,a,b,c)
#define LOG4(ctx,level,fmt,a,b,c,d)
#define LOG5(ctx,level,fmt,a,b,c,d,e)

#else

#define LOG0(ctx,level,fmt) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt);
#define LOG1(ctx,level,fmt,a) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt,(ULONG)a);
#define LOG2(ctx,level,fmt,a,b) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt,(ULONG)a,(ULONG)b);
#define LOG3(ctx,level,fmt,a,b,c) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt,(ULONG)a,(ULONG)b,(ULONG)c);
#define LOG4(ctx,level,fmt,a,b,c,d) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt,(ULONG)a,(ULONG)b,(ULONG)c,(ULONG)d);
#define LOG5(ctx,level,fmt,a,b,c,d,e) APB_Log(ctx,__FILE__,__LINE__,__FUNC__,level,fmt,(ULONG)a,(ULONG)b,(ULONG)c,(ULONG)d,(ULONG)e);

#endif

#endif // __AMIPIBORG_H__
