
#ifndef __LIB_H__
#define __LIB_H__

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <utility/hooks.h>

#include "amipiborg.h"
#include "amipiborg_protos.h"

#define CNN_MSG_TIMEOUT 5

struct AmiPiBorgLibrary {
    struct Library l_Lib;
    UWORD     l_Pad;
    BPTR      l_SegList;
    struct Library *l_ExecBase;
    struct Library *l_UtilityBase;
    struct Library *l_DOSBase;
};

struct ApbContext {
    APTR      ctx_MemPool;
    APTR      ctx_Logger;
    APTR      ctx_StatColl;
    APTR      ctx_ObjPool;
};

struct Connection {
    struct MsgPort *c_MsgPort;
    struct ApbContext *c_Ctx;
    UWORD     c_Status;
    UWORD     c_HandlerId;
    UWORD     c_ConnId;
};

struct RequestInt {
    struct APBRequest r_Req;
    struct Connection *r_Conn;
    UWORD     r_TimeoutTicks;
};


BOOL      APB_PutMsg(
    struct ApbContext *ctx,
    struct Message *msg);


/** Memory Management **/

APTR      APB_AllocMemInternal(
    APTR memPool,
    ULONG size);

VOID      APB_FreeMemInternal(
    APTR memPool,
    APTR memory,
    ULONG size);

/** Logging **/

APTR      APB_CreateLogger(
    APTR memPool,
    BPTR outputFh,
    UWORD level);

VOID      APB_FreeLogger(
    APTR logger);

VOID      APB_LogInternalArgArray(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    APTR argarray);

VOID      APB_Log(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...);

VOID      APB_LogMemInternal(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    BYTE * data,
    UWORD length);

BOOL      APB_ShouldLogInternal(
    APTR logger,
    UWORD level);

#ifdef APB_NOLOG

#define LOGI0(l,level,msg)
#define LOGI1(l,level,msg,a)
#define LOGI2(l,level,msg,a,b)
#define LOGI3(l,level,msg,a,b,c)
#define LOGI4(l,level,msg,a,b,c,d)

#else

#define LOGI0(l,level,msg) if( APB_ShouldLogInternal(l, level) ) APB_Log(l, __FILE__, __LINE__, __FUNC__, level, msg)
#define LOGI1(l,level,msg,a) if( APB_ShouldLogInternal(l, level) ) APB_Log(l, __FILE__, __LINE__, __FUNC__, level, msg, (ULONG)a)
#define LOGI2(l,level,msg,a,b) if( APB_ShouldLogInternal(l, level) ) APB_Log(l, __FILE__, __LINE__, __FUNC__, level, msg, (ULONG)a, (ULONG)b)
#define LOGI3(l,level,msg,a,b,c) if( APB_ShouldLogInternal(l, level) ) APB_Log(l, __FILE__, __LINE__, __FUNC__, level, msg, (ULONG)a, (ULONG)b, (ULONG)c)
#define LOGI4(l,level,msg,a,b,c,d) if( APB_ShouldLogInternal(l, level) ) APB_Log(l, __FILE__, __LINE__, __FUNC__, level, msg, (ULONG)a, (ULONG)b, (ULONG)c, (ULONG)d)

#endif

/** Stat Collection **/

APTR      APB_CreateStatCollector(
    APTR memPool);

VOID      APB_FreeStatCollector(
    APTR statCollector);

/** Object Pool **/

APTR      APB_CreateObjectPool(
    APTR memPool,
    APTR log);

VOID      APB_FreeObjectPool(
    APTR pool);

BOOL      APB_TypeRegisteredInternal(
    APTR pool,
    UWORD id);

VOID      APB_RegisterObjectTypeInternal(
    APTR pool,
    UWORD id,
    STRPTR name,
    UWORD size,
    UWORD minInstances,
    UWORD maxInstances);

APTR      APB_AllocObjectInternal(
    APTR pool,
    UWORD id);

VOID      APB_FreeObjectInternal(
    APTR pool,
    UWORD id,
    APTR obj);

enum {
    OT_CONNECTION = 0xffff,
    OT_REQUEST = 0xfffe
};

#endif
