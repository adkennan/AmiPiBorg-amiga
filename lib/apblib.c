
#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdarg.h>

VOID APB_Log(
    APTR ctx,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...)
{
    APTR      args;

    if(APB_ShouldLog(ctx, level)) {

        args = (APTR) APB_PointerAdd(&fmt, sizeof(STRPTR));

        APB_LogArgArray(ctx, file, line, func, level, fmt, args);

    }
}

WORD APB_GetLogLevel(
    STRPTR levelName)
{
    if(levelName[0] == 'E' || levelName[0] == 'e') {
        return LOG_ERROR;
    } else if(levelName[0] == 'I' || levelName[0] == 'i') {
        return LOG_INFO;
    } else if(levelName[0] == 'D' || levelName[0] == 'd') {
        return LOG_DEBUG;
    } else if(levelName[0] == 'T' || levelName[0] == 't') {
        return LOG_TRACE;
    }

    return -1;
}
