
#include "config.h"

#include "log.h"

#include <string.h>

#include <dos/dos.h>

#include <clib/dos_protos.h>

#define ARG_LOGLEVEL 0
#define ARG_QUIET 1
#define ARG_REMOTE 2
#define ARG_REMARGS 3
#define ARGS_LEN 4

#define TEMPLATE "L=LogLevel/K,Q=Quiet/S,R=Remote/K,RemoteArgs/F"

VOID APB_ShowError(STRPTR msg) 
{
	if( msg ) {
		PutStr(msg);
		PutStr("\n");
	}

	PutStr("Format: ");
	PutStr(TEMPLATE);
	PutStr("\n");
}

struct Config *APB_GetConfig(MemoryPool mp) 
{
	struct RDArgs *rd;
	struct Config *cfg = NULL;
	WORD logLevel = LOG_INFO;
	BOOL logToStdOut = TRUE;
	UWORD len1, len2 = 0;

	LONG argArr[ARGS_LEN] = {
		(LONG)"I",
		0,
		(LONG)"serialremote.library",
		0
	};

	if( ! (rd = ReadArgs(TEMPLATE, argArr, NULL) ) ) {
		APB_ShowError("Invalid Arguments");
		goto done;
	}

	if( argArr[ARG_LOGLEVEL] ) {
		logLevel = APB_GetLogLevel((STRPTR)argArr[ARG_LOGLEVEL]);
		if( logLevel < 0 ) {
			APB_ShowError("Invalid Log Level. Valid values are E, I, D or T.");
			goto done;
		}
	}

	if( argArr[ARG_QUIET] ) {
		logToStdOut = FALSE;
	}

	len1 = strlen((STRPTR)argArr[ARG_REMOTE]) + 1;
	if( len1 % 2 ) {
		len1++;
	}

	if( argArr[ARG_REMARGS] ) {
		len2 = strlen((STRPTR)argArr[ARG_REMARGS]) + 1;
		if( len2 % 2 ) {
			len2++;
		}
	}

	if( ! (cfg = (struct Config *)APB_AllocMem(mp, sizeof(struct Config) + len1 + len2) ) ) {
		APB_ShowError("Unable to allocate config structure.");
		goto done;
	}
 
	cfg->cf_MemPool = mp;
	cfg->cf_Size = len1 + len2;
	cfg->cf_LogLevel = logLevel;
	cfg->cf_LogToStdOut = logToStdOut;
	cfg->cf_RemoteName = (STRPTR)APB_PointerAdd(cfg, sizeof(struct Config));
	strcpy(cfg->cf_RemoteName, (STRPTR)argArr[ARG_REMOTE]);

	if( len2 ) {
		cfg->cf_RemoteArgs = (STRPTR)APB_PointerAdd(cfg, sizeof(struct Config) + len1);
		strcpy(cfg->cf_RemoteArgs, (STRPTR)argArr[ARG_REMARGS]);
	}

done:
	if( rd ) {
		FreeArgs(rd);
	}

	return cfg;
}

VOID APB_FreeConfig(struct Config *cfg)
{
	APB_FreeMem(cfg->cf_MemPool, cfg, cfg->cf_Size);
}

BOOL APB_ParseRemoteArgs(struct Config *cfg, struct RemoteArgs *ra)
{
	struct RDArgs *rda;
	BOOL result = FALSE;

	if( rda = (struct RDArgs *)AllocDosObject(DOS_RDARGS, NULL) ) {

		rda->RDA_Source.CS_Buffer = cfg->cf_RemoteArgs;
		rda->RDA_Source.CS_Length = strlen(cfg->cf_RemoteArgs);

		if( ReadArgs(ra->ra_Template, ra->ra_ArgValues, rda) ) {

			result = TRUE;

			FreeArgs(rda);
		} else {
			PutStr("Error parsing args for ");
			PutStr(cfg->cf_RemoteName);
			PutStr(": ");
			PutStr(ra->ra_Template);
			PutStr("\n");
		}

		FreeDosObject(DOS_RDARGS, rda);
	}

	return result;
}
