#include "log.h"

#include <string.h>
#include <stdarg.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

STRPTR    separator = "::";

#define PAD_ZERO 1
#define PAD_RIGHT 2
#define SIGNED 3

#define NUM_BUF_LEN 64
char      numBuf[NUM_BUF_LEN];

struct Logger {
    APTR      l_MemPool;
    UWORD     l_Level;
    UWORD     l_BufSize;
    BYTE     *l_Buf;
    UWORD     l_Start;
    UWORD     l_End;
    BOOL      l_LogToStdOut;
};

struct Logger Log;

VOID APB_InitLog(
    MemoryPool memPool,
    UWORD level,
    UWORD logBufSize,
    BOOL logToStdOut)
{
    Log.l_MemPool = memPool;
    Log.l_Level = level;
    Log.l_BufSize = logBufSize;
    Log.l_LogToStdOut = logToStdOut;

    Log.l_Buf = APB_AllocMem(memPool, logBufSize + 1);
    Log.l_Buf[logBufSize] = '\0';
    Log.l_Start = 0;
    Log.l_End = 0;
}

VOID APB_DestroyLog(
    VOID)
{
    APB_FreeMem(Log.l_MemPool, Log.l_Buf, Log.l_BufSize + 1);
}

BOOL APB_ShouldLog(
    UWORD level)
{
    if(level <= Log.l_Level) {
        return TRUE;
    }
    return FALSE;
}

VOID APB_LogByte(
    BYTE c)
{
    Log.l_Buf[Log.l_End] = c;
    Log.l_End++;

    if(Log.l_End >= Log.l_BufSize) {
        Log.l_End = 0;
    }

    if(Log.l_End == Log.l_Start) {
        do {
            Log.l_Start++;
            if(Log.l_Start == Log.l_BufSize) {
                Log.l_Start = 0;
            }
        } while(Log.l_Buf[Log.l_Start]);
        Log.l_Start++;
        if(Log.l_Start == Log.l_BufSize) {
            Log.l_Start = 0;
        }
    }
}

UWORD APB_LogString(
    STRPTR s,
    UWORD width,
    UWORD flags)
{
    UWORD     c = 0;
    BYTE      pad = ' ';
    UWORD     l;

    if(width > 0) {
        l = strlen(s);
        if(l >= width) {
            width = 0;
        } else {
            width -= l;
        }
        if(flags & PAD_ZERO) {
            pad = '0';
        }
    }

    if(!(flags & PAD_RIGHT)) {
        while(width > 0) {
            APB_LogByte(pad);
            c++;
            width--;
        }
    }

    while(*s) {
        APB_LogByte(*s);
        s++;
        c++;
    }

    while(width > 0) {
        APB_LogByte(pad);
        c++;
        width--;
    }
    return c;
}

UWORD APB_LogNumber(
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
        numBuf[0] = '0';
        numBuf[1] = '\0';
        return APB_LogString(numBuf, width, flags);
    }

    if((flags & SIGNED) && base == 10 && v < 0) {
        neg = TRUE;
        u = -v;
    }

    s = numBuf + NUM_BUF_LEN - 1;
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

    return APB_LogString(s, width, flags);
}

UWORD APB_LogLevel(
    UWORD level)
{
    if(level == LOG_ERROR) {
        APB_LogString("E::", 0, 0);
    } else if(level <= LOG_INFO) {
        APB_LogString("I::", 0, 0);
    } else if(level <= LOG_DEBUG) {
        APB_LogString("D::", 0, 0);
    } else {
        APB_LogString("T::", 0, 0);
    }

    return 3;
}


VOID APB_SetLogLevel(
    UWORD level)
{
    Log.l_Level = level;
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

UWORD APB_CopyLog(
    BYTE * data,
    UWORD length)
{
    UWORD     dataLen, s, l;

    if(Log.l_Start > Log.l_End) {

        dataLen = Log.l_BufSize - (Log.l_Start - Log.l_End);
        if(dataLen > length) {
            dataLen = length;
        }
        s = Log.l_Start;
        l = Log.l_BufSize - Log.l_Start;

        CopyMem(APB_PointerAdd(Log.l_Buf, s), data, l);

        s = 0;
        l = Log.l_End;

        CopyMem(Log.l_Buf, APB_PointerAdd(data, Log.l_BufSize - Log.l_Start), Log.l_End);

    } else {
        dataLen = Log.l_End - Log.l_Start;
        if(dataLen > length) {
            dataLen = length;
        }

        CopyMem(APB_PointerAdd(Log.l_Buf, Log.l_Start), data, dataLen);
    }

    return dataLen;
}



VOID APB_Log(
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    STRPTR fmt,
    ...)
{
    va_list   ap;
    UWORD     width;
    UWORD     flags;
    UWORD     s = Log.l_End;
    UWORD     c;

    c = APB_LogLevel(level);
    c += APB_LogString(file, 0, 0);
    APB_LogByte('(');
    c++;
    c += APB_LogNumber(line, 10, 0, 0, 'a');
    APB_LogByte(')');
    c++;
    c += APB_LogString(separator, 0, 0);
    c += APB_LogString(func, 0, 0);
    c += APB_LogString(separator, 0, 0);

    va_start(ap, fmt);

    for(; *fmt; ++fmt) {
        if(*fmt == '%') {
            width = 0;
            flags = 0;

            fmt++;

            if(*fmt == '\0') {
                break;
            }

            if(*fmt == '%') {
                APB_LogByte('%');
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
                c += APB_LogNumber(va_arg(ap, ULONG), 10, width, flags | SIGNED, 'a');
                break;

            case 'u':
                c += APB_LogNumber(va_arg(ap, ULONG), 10, width, flags, 'a');
                break;

            case 'x':
                c += APB_LogNumber(va_arg(ap, ULONG), 16, width, flags, 'a');
                break;

            case 'X':
                c += APB_LogNumber(va_arg(ap, ULONG), 16, width, flags, 'A');
                break;

            case 'c':
                numBuf[0] = (BYTE) va_arg(ap, ULONG);
                if(numBuf[0] < ' ') {
                    numBuf[0] = '.';
                }
                numBuf[1] = '\0';
                c += APB_LogString(numBuf, width, flags);
                break;

            case 's':
                c += APB_LogString(va_arg(ap, STRPTR), width, flags);
                break;

            default:
                break;
            }
        } else {
            c++;
            APB_LogByte(*fmt);
        }
    }

    va_end(ap);

    APB_LogByte('\0');

    if(Log.l_LogToStdOut) {

        PutStr((STRPTR) (Log.l_Buf + s));

        if(Log.l_End < s) {
            PutStr((STRPTR) Log.l_Buf);
        }
        PutStr("\n");
    }
}

VOID APB_LogMem(
    STRPTR file,
    UWORD line,
    STRPTR func,
    UWORD level,
    BYTE * data,
    UWORD length)
{
    while(length > 0) {

        switch (length) {
        case 1:
            APB_Log(file, line, func, level, "%02x                   %c", *data, *data);
            length -= 1;
            break;
        case 2:
            APB_Log(file, line, func, level, "%02x %02x                %c%c", *data, *(data + 1), *data, *(data + 1));
            length -= 2;
            break;
        case 3:
            APB_Log(file, line, func, level, "%02x %02x %02x             %c%c%c", *data, *(data + 1), *(data + 2), *data, *(data + 1), *(data + 2));
            length -= 3;
            break;
        case 4:
            APB_Log(file, line, func, level, "%02x %02x %02x %02x              %c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *data, *(data + 1), *(data + 2), *(data + 3));
            length -= 4;
            break;
        case 5:
            APB_Log(file, line, func, level, "%02x %02x %02x %02x %02x           %c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4));
            length -= 5;
            break;
        case 6:
            APB_Log(file, line, func, level, "%02x %02x %02x %02x %02x %02x        %c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5));
            length -= 6;
            break;
        case 7:
            APB_Log(file, line, func, level, "%02x %02x %02x %02x %02x %02x %02x     %c%c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6),
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6));
            length -= 7;
            break;
        default:
            APB_Log(file, line, func, level, "%02x %02x %02x %02x %02x %02x %02x %02x  %c%c%c%c%c%c%c%c",
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6), *(data + 7),
                    *data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5), *(data + 6), *(data + 7));
            data += 8;
            length -= 8;
            break;
        }
    }
}
