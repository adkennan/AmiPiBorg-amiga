
#ifndef __APB_REMOTE_PROTOS_H__
#define __APB_REMOTE_PROTOS_H__

struct RemoteArgs
{
	STRPTR ra_Template;
	UWORD ra_ArgCount;
	LONG *ra_ArgValues;
};

APTR __saveds __asm REM_CreateRemote(VOID);

VOID __saveds __asm REM_DestroyRemote(
	register __a0 APTR remote
);

struct RemoteArgs * __saveds __asm REM_GetArgTemplate(
	register __a0 APTR remote
);

VOID __saveds __asm REM_ConfigureRemote(
	register __a0 APTR remote,
	register __a1 struct RemoteArgs *ra
);

BOOL __saveds __asm REM_OpenRemote(
	register __a0 APTR remote
);

VOID __saveds __asm REM_CloseRemote(
	register __a0 APTR remote
);

ULONG __saveds __asm REM_GetSignalBits(
	register __a0 APTR remote
);

VOID __saveds __asm REM_HandleSignal(
	register __a0 APTR remote,
	register __d0 ULONG signalBits
);

BOOL __saveds __asm REM_CanRead(
	register __a0 APTR remote
);

VOID __saveds __asm REM_Read(
	register __a0 APTR remote,
	register __a1 BYTE *data,
	register __d0 UWORD length
);

UWORD __saveds __asm REM_BytesRead(
	register __a0 APTR remote
);

BOOL __saveds __asm REM_CanWrite(
	register __a0 APTR remote
);

VOID __saveds __asm REM_Write(
	register __a0 APTR remote,
	register __a1 BYTE *data,
	register __d0 UWORD length
);

#endif // __APB_REMOTE_PROTOS_H__