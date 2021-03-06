
#include "config.h"

#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdio.h>
#include <string.h>

#include <dos/dos.h>

#include <clib/dos_protos.h>

#define ARG_LOGLEVEL 0
#define ARG_REMOTE 1
#define ARG_REMARGS 2
#define ARGS_LEN 3

#define TEMPLATE "L=LOGLEVEL/K,R=REMOTE/K,REMOTEARGS/F"

VOID APB_ShowError(
    STRPTR msg)
{
    if(msg) {
        PutStr(msg);
        PutStr("\n");
    }

    PutStr("Format: ");
    PutStr(TEMPLATE);
    PutStr("\n");
}

struct Config *APB_GetConfig(
    APTR ctx)
{
    struct RDArgs *rd;
    struct Config *cfg = NULL;
    WORD      logLevel = LOG_INFO;
    UWORD     len1, len2 = 0;

    LONG      argArr[ARGS_LEN] = {
        (LONG) "I",
        (LONG) "serialremote.library",
        0
    };

    if(!(rd = ReadArgs(TEMPLATE, argArr, NULL))) {
        APB_ShowError("Invalid Arguments");
        goto done;
    }

    if(argArr[ARG_LOGLEVEL]) {
        logLevel = APB_GetLogLevel((STRPTR) argArr[ARG_LOGLEVEL]);
        if(logLevel < 0) {
            APB_ShowError("Invalid Log Level. Valid values are E, I, D or T.");
            goto done;
        }
    }

    len1 = strlen((STRPTR) argArr[ARG_REMOTE]) + 1;
    if(len1 % 2) {
        len1++;
    }

    if(argArr[ARG_REMARGS]) {
        len2 = strlen((STRPTR) argArr[ARG_REMARGS]) + 2;
    }

    if(!(cfg = (struct Config *) APB_AllocMem(ctx, sizeof(struct Config) + len1 + len2))) {
        APB_ShowError("Unable to allocate config structure.");
        goto done;
    }

    cfg->cf_Ctx = ctx;
    cfg->cf_Size = len1 + len2;

    APB_SetLogLevel(ctx, logLevel);

    cfg->cf_RemoteName = (STRPTR) APB_PointerAdd(cfg, sizeof(struct Config));
    strcpy(cfg->cf_RemoteName, (STRPTR) argArr[ARG_REMOTE]);

    if(len2) {
        cfg->cf_RemoteArgs = (STRPTR) APB_PointerAdd(cfg, sizeof(struct Config) + len1);
        strcpy(cfg->cf_RemoteArgs, (STRPTR) argArr[ARG_REMARGS]);
        cfg->cf_RemoteArgs[len2 - 2] = '\n';
        cfg->cf_RemoteArgs[len2 - 1] = '\0';
    } else {
        cfg->cf_RemoteArgs = NULL;
    }

  done:
    if(rd) {
        FreeArgs(rd);
    }

    return cfg;
}

VOID APB_FreeConfig(
    struct Config * cfg)
{
    APB_FreeMem(cfg->cf_Ctx, cfg, sizeof(struct Config) + cfg->cf_Size);
}

BOOL APB_ConfigureRemote(
    struct Config *cfg,
    Remote rem)
{
    struct RDArgs *rda;
    BOOL      result = TRUE;
    struct RemoteArgs *ra = REM_GetArgTemplate(rem);

    if(ra) {

        if(cfg->cf_RemoteArgs && strlen(cfg->cf_RemoteArgs) > 0) {

            LOG1(cfg->cf_Ctx, LOG_DEBUG, "Confguring remote with args \"%s\"", cfg->cf_RemoteArgs);

            if(rda = (struct RDArgs *) AllocDosObject(DOS_RDARGS, NULL)) {

                rda->RDA_Source.CS_Buffer = cfg->cf_RemoteArgs;
                rda->RDA_Source.CS_Length = strlen(cfg->cf_RemoteArgs);

                if(ReadArgs(ra->ra_Template, ra->ra_ArgValues, rda)) {

                    if(!REM_ConfigureRemote(rem, ra)) {

                        LOG0(cfg->cf_Ctx, LOG_ERROR, "Failed to configure remote.");
                        result = FALSE;
                    }

                    FreeArgs(rda);

                } else {

                    LOG0(cfg->cf_Ctx, LOG_ERROR, "Failed to parse args for remote");

                    result = FALSE;

                    PutStr("Error parsing args for ");
                    PutStr(cfg->cf_RemoteName);
                    PutStr(": ");
                    PutStr(ra->ra_Template);
                    PutStr("\n");
                }

                FreeDosObject(DOS_RDARGS, rda);

            } else {

                LOG0(cfg->cf_Ctx, LOG_ERROR, "Failed to allocate RDArgs structure.");

                result = FALSE;
            }
        }
    }

    return result;
}
