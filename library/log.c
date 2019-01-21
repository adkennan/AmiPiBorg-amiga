
#include "lib.h"

#include <string.h>
#include <stdarg.h>

#include <utility/utility.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

STRPTR    separator = "::";

#define PAD_ZERO 1
#define PAD_RIGHT 2
#define SIGNED 3

#define NUM_BUF_LEN 64
#define LOG_BUF_LEN 1024

struct Logger {
    APTR      l_MemPool;
    BPTR      l_OutputFh;
    UWORD     l_Level;
    BYTE     *l_Buf;
    UWORD     l_End;
    UBYTE     l_NumBuf[NUM_BUF_LEN];
};

APTR APB_CreateLogger(
    APTR memPool,
    BPTR outputFh,
    UWORD level)
{
    struct Logger *l;
    UWORD ix;

    if( l = (struct Logger *)APB_AllocMemInternal(memPool, sizeof(struct Logger))) {

        l->l_MemPool = memPool;
        l->l_OutputFh = outputFh;
        l->l_Level = level;
        l->l_Buf = APB_AllocMemInternal(memPool, LOG_BUF_LEN + 1);
        l->l_End = 0;
    }

    for( ix = 0; ix <= LOG_BUF_LEN; ix++ ) {
        l->l_Buf[ix] = '\0';
    }
    
    return l;
}

VOID APB_FreeLogger(
    APTR logger)
{
    struct Logger *l = (struct Logger *)logger;

    if( l->l_Buf ) {
        APB_FreeMemInternal(l->l_MemPool, l->l_Buf, LOG_BUF_LEN + 1);
    }

    APB_FreeMemInternal(l->l_MemPool, l, sizeof(struct Logger));
}

BOOL APB_ShouldLogInternal(
    APTR logger,
    UWORD level)
{
    struct Logger *l = (struct Logger *)logger;

    if(level <= l->l_Level && l->l_OutputFh ) {
        return TRUE;
    }
    return FALSE;
}

BOOL __asm __saveds APB_ShouldLog(
	register __a0 APTR ctx,
	register __d0 UWORD level)
{
    struct Logger *logger = (struct Logger *)((struct ApbContext *)ctx)->ctx_Logger;
 
    return APB_ShouldLogInternal(logger, level);   
}

VOID APB_LogByte(
    struct Logger *l,
    BYTE c)
{
    if( l->l_End >= LOG_BUF_LEN ) {
        return;
    }

    l->l_Buf[l->l_End] = c;
    l->l_End++;
}

UWORD APB_LogString(
    struct Logger *l,
    STRPTR s,
    UWORD width,
    UWORD flags)
{
    UWORD     c = 0;
    BYTE      pad = ' ';
    UWORD     len;

    if(width > 0) {
        len = strlen(s);
        if(len >= width) {
            width = 0;
        } else {
            width -= len;
        }
        if(flags & PAD_ZERO) {
            pad = '0';
        }
    }

    if(!(flags & PAD_RIGHT)) {
        while(width > 0) {
            APB_LogByte(l, pad);
            c++;
            width--;
        }
    }

    while(*s) {
        APB_LogByte(l, *s);
        s++;
        c++;
    }

    while(width > 0) {
        APB_LogByte(l, pad);
        c++;
        width--;
    }
    return c;
}

UWORD APB_LogNumber(
    struct Logger *l,
    LONG v,
    UWORD base,
    UWORD width,
    UWORD flags,
    BYTE letBase)
{
    STRPTR    s;
    BYTE      d;
    ULONG     u = v;
    BOOL      neg = FALSE;

    if(v == 0) {
        l->l_NumBuf[0] = '0';
        l->l_NumBuf[1] = '\0';
        return APB_LogString(l, l->l_NumBuf, width, flags);
    }

    if((flags & SIGNED) && base == 10 && v < 0) {
        neg = TRUE;
        u = -v;
    }

    s = l->l_NumBuf + NUM_BUF_LEN - 1;
    *s = '\0';

    while(u) {
        d = u % base;
        if(d >= 10) {
            d += letBase - '0' - 10;
        }
        *--s = d + '0';
        u /= base;
    }

    if(neg) {
        *--s = '-';

    }

    return APB_LogString(l, s, width, flags);
}

UWORD APB_LogLevel(
    struct Logger *l,
    UWORD level)
{
    if(level == LOG_ERROR) {
        APB_LogString(l, "E::", 0, 0);
    } else if(level <= LOG_INFO) {
        APB_LogString(l, "I::", 0, 0);
    } else if(level <= LOG_DEBUG) {
        APB_LogString(l, "D::", 0, 0);
    } else {
        APB_LogString(l, "T::", 0, 0);
    }

    return 3;
}


VOID __asm __saveds APB_SetLogLevel(
    register __a0 APTR ctx,
    register __d0 UWORD level)
{

    struct Logger *l = (struct Logger *)((struct ApbContext *)ctx)->ctx_Logger;
    l->l_Level = level;
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

#define GETARG(a) ((ULONG)(*(ULONG *)((a += 1 ) - 1)))

VOID APB_LogArgArrayInternal(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    APTR args)
{
    UWORD     width;
    UWORD     flags;
    UWORD     c;
    struct Logger *l = (struct Logger *)logger;
    ULONG *aa = (ULONG *)args;
    
    if( ! l->l_OutputFh ) {
        return;
    }

    l->l_End = 0;

    c = APB_LogLevel(l, level);
    c += APB_LogString(l, file, 0, 0);
    APB_LogByte(l, '(');
    c++;
    c += APB_LogNumber(l, line, 10, 0, 0, 'a');
    APB_LogByte(l, ')');
    c++;
    c += APB_LogString(l, separator, 0, 0);
    c += APB_LogString(l, func, 0, 0);
    c += APB_LogString(l, separator, 0, 0);

    for(; *fmt; ++fmt) {
        if(*fmt == '%') {
            width = 0;
            flags = 0;

            fmt++;

            if(*fmt == '\0') {
                break;
            }

            if(*fmt == '%') {
                APB_LogByte(l, '%');
                c++;
                continue;
            }

            while(*fmt == '0') {
                flags |= PAD_ZERO;
                fmt++;
            }

            for(; *fmt >= '0' && *fmt <= '9'; ++fmt) {
                width *= 10;
                width += *fmt - '0';
            }

            switch (*fmt) {
            case 'd':
                c += APB_LogNumber(l, GETARG(aa), 10, width, flags | SIGNED, 'a');
                break;

            case 'u':
                c += APB_LogNumber(l, GETARG(aa), 10, width, flags, 'a');
                break;

            case 'x':
                c += APB_LogNumber(l, GETARG(aa), 16, width, flags, 'a');
                break;

            case 'X':
                c += APB_LogNumber(l, GETARG(aa), 16, width, flags, 'A');
                break;

            case 'c':
                l->l_NumBuf[0] = (BYTE)GETARG(aa);
                if(l->l_NumBuf[0] < ' ') {
                    l->l_NumBuf[0] = '.';
                }
                l->l_NumBuf[1] = '\0';
                c += APB_LogString(l, l->l_NumBuf, width, flags);
                break;

            case 's':
                c += APB_LogString(l, (STRPTR)GETARG(aa), width, flags);
                break;

            default:
                break;
            }
        } else {
            c++;
            APB_LogByte(l, *fmt);
        }
    }

    APB_LogByte(l, '\n');
    APB_LogByte(l, '\0');

    if( l->l_OutputFh ) {
        FPuts(l->l_OutputFh, (STRPTR)l->l_Buf);
    }
}

VOID APB_Log(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...)
{
    APTR args = (APTR)APB_PointerAdd(&fmt,sizeof(STRPTR));      

    APB_LogArgArrayInternal(
        logger, file, line, func, level, fmt, args);
}

VOID __asm __saveds APB_LogArgArray(
    register __a0 APTR ctx,
    register __a1 STRPTR file,
    register __d0 UWORD line,
    register __a2 STRPTR func,
    register __d1 UWORD level,
    register __a5 STRPTR fmt,
    register __d2 APTR args
)
{
    struct ApbContext *context = (struct ApbContext *)ctx;
    struct Logger *logger = (struct Logger *)context->ctx_Logger;
    
    APB_LogArgArrayInternal(
        logger, file, line, func, level, fmt, args);    
}

VOID APB_LogMemInternal(
    APTR logger,
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    BYTE * data,
    UWORD length)
{
    return;

    while(length > 0) {

        switch (length) {
        case 1:
            APB_Log(logger, file, line, func, level, "%02x                   %c", *data, *data);
            length -= 1;
            break;
        case 2:
            APB_Log(logger, file, line, func, level, "%02x %02x                %c%c", *data, *(data + 1), *data, *(data + 1));
            length -= 2;
            break;
        case 3:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x             %c%c%c", *data, *(data + 1), *(data + 2), *data, *(data + 1), *(data + 2));
            length -= 3;
            break;
        case 4:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x %02x              %c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *data, *(data + 1), *(data + 2), *(data + 3));
            length -= 4;
            break;
        case 5:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x %02x %02x           %c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4));
            length -= 5;
            break;
        case 6:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x %02x %02x %02x        %c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5));
            length -= 6;
            break;
        case 7:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x %02x %02x %02x %02x     %c%c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6),
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6));
            length -= 7;
            break;
        default:
            APB_Log(logger, file, line, func, level, "%02x %02x %02x %02x %02x %02x %02x %02x  %c%c%c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6), *(data + 7),
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6), *(data + 7));
            data += 8;
            length -= 8;
            break;
        }
    }
}
