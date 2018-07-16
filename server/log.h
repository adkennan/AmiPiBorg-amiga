#ifndef __APB_LOG_H__
#define __ABP_LOG_H__

#include <exec/types.h>

#include "memory.h"

VOID      APB_InitLog(
    MemoryPool memPool,
    UWORD level,
    UWORD logBufSize,
    BOOL logToStdOut);

VOID      APB_DestroyLog(
    VOID);

BOOL      APB_ShouldLog(
    UWORD level);

VOID      APB_Log(
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...);

VOID      APB_LogMem(
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    BYTE * data,
    UWORD length);

WORD      APB_GetLogLevel(
    STRPTR levelName);

#define LOG_ERROR 0
#define LOG_INFO 5
#define LOG_DEBUG 10
#define LOG_TRACE 20

#define LOG0(level,msg) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg)
#define LOG1(level,msg,a) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a)
#define LOG2(level,msg,a,b) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b)
#define LOG3(level,msg,a,b,c) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c)
#define LOG4(level,msg,a,b,c,d) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d)
#define LOG5(level,msg,a,b,c,d,e) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d, e)
#define LOG6(level,msg,a,b,c,d,e,f) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d, e, f)
#define LOG7(level,msg,a,b,c,d,e,f,g) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d, e, f, g)
#define LOG8(level,msg,a,b,c,d,e,f,g,h) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d, e, f, g, h)
#define LOG9(level,msg,a,b,c,d,e,f,g,h,i) if( APB_ShouldLog(level) ) APB_Log(__FILE__, __LINE__, __FUNC__, level, msg, a, b, c, d, e, f, g, h, i)

#endif
