
#ifndef __APB_PROTOS_H__
#define __APB_PROTOS_H__


#include <dos/dos.h>

#include <stdarg.h>

APTR __asm APB_CreateContext(
    register __a0 APTR memPool,
    register __a1 BPTR logFh,
    register __d0 UWORD logLevel);

VOID __asm __saveds APB_FreeContext(
    register __a0 APTR ctx);

APTR __asm __saveds APB_AllocConnection(
    register __a0 APTR ctx,
    register __a1 struct MsgPort *port,
    register __d0 UWORD handlerId);

VOID __asm __saveds APB_FreeConnection(
    register __a0 APTR connection);

BOOL __asm __saveds APB_OpenConnection(
    register __a0 APTR connection);

VOID __asm __saveds APB_CloseConnection(
    register __a0 APTR connection);

UWORD __asm __saveds APB_ConnectionState(
    register __a0 APTR connection);

struct APBRequest *__asm __saveds APB_AllocRequest(
    register __a0 APTR connection);

VOID __asm __saveds APB_FreeRequest(
    register __a0 struct APBRequest *request);


VOID __asm __saveds APB_Read(
    register __a0 struct APBRequest *request);

VOID __asm __saveds APB_Write(
    register __a0 struct APBRequest *request);

VOID __asm __saveds APB_Abort(
    register __a0 struct APBRequest *request);

// Control Functions

UWORD __asm __saveds APB_SrvQuit(
    VOID);

UWORD __asm __saveds APB_SrvSetLogLevel(
    register __d0 UWORD level);

UWORD __asm __saveds APB_SrvGetLog(
    register __a0 BYTE * data,
    register __d0 UWORD length);

UWORD __asm __saveds APB_SrvGetConnections(
    register __a0 BYTE * data,
    register __d0 UWORD length);

UWORD __asm __saveds APB_SrvGetStats(
    register __a0 BYTE * data,
    register __d0 UWORD length);

// Statistics

VOID __asm __saveds APB_RegisterStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType);

VOID __asm __saveds APB_IncrementStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType,
    register __d1 LONG value);

LONG __asm __saveds APB_GetStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType);

// Logging

VOID __asm __saveds APB_SetLogLevel(
    register __a0 APTR ctx,
    register __d0 UWORD logLevel);

VOID __asm __saveds APB_LogArgArray(
    register __a0 APTR ctx,
    register __a1 STRPTR file,
    register __d0 UWORD line,
    register __a2 STRPTR func,
    register __d1 UWORD level,
    register __a5 STRPTR fmt,
    register __d2 APTR args);

BOOL __asm __saveds APB_ShouldLog(
    register __a0 APTR ctx,
    register __d0 UWORD level);

VOID      APB_Log(
    APTR ctx,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...);

WORD      APB_GetLogLevel(
    STRPTR levelName);

// Memory

APTR __asm __saveds APB_AllocMem(
    register __a0 APTR ctx,
    register __d0 ULONG size);

VOID __asm __saveds APB_FreeMem(
    register __a0 APTR ctx,
    register __a1 APTR memory,
    register __d0 ULONG size);

// Object Pool

BOOL __asm __saveds APB_TypeRegistered(
    register __a0 APTR ctx,
    register __d0 UWORD id);

VOID __asm __saveds APB_RegisterObjectType(
    register __a0 APTR ctx,
    register __d0 UWORD id,
    register __a1 STRPTR name,
    register __d1 UWORD size,
    register __d2 UWORD minInstances,
    register __d3 UWORD maxInstances);

APTR __asm __saveds APB_AllocObject(
    register __a0 APTR ctx,
    register __d0 UWORD id);

VOID __asm __saveds APB_FreeObject(
    register __a0 APTR ctx,
    register __d0 UWORD id,
    register __a1 APTR obj);

#endif // __APB_PROTOS_H__
