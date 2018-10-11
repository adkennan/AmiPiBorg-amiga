#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <dos/dos.h>

#include <proto/exec.h>

/**** Main Library Structure ****/

struct RemoteLibrary {
    struct Library r_Lib;
    UWORD     r_Pad;
    BPTR      r_SegList;
    struct Library *r_ExecBase;
#ifdef REMOTE_LIB_DATA
    struct REMOTE_LIB_DATA r_Data;
#endif
};

/**** Initialization Structs ****/
struct InitTable;
struct InitDataTable;


/**** Library Init ****/
struct RemoteLibrary *__saveds __asm InitLib(
    register __a6 struct Library *sysBase,
    register __a0 BPTR segList,
    register __d0 struct RemoteLibrary *cb);


/**** Library Functions ****/
struct RemoteLibrary *__saveds __asm _LibOpen(
    register __a6 struct RemoteLibrary *RemoteLibraryBase);

BPTR __saveds __asm _LibClose(
    register __a6 struct RemoteLibrary *RemoteLibraryBase);

BPTR __saveds __asm _LibExpunge(
    register __a6 struct RemoteLibrary *RemoteLibraryBase);

ULONG __saveds __asm ExtFuncLib(
    register __a6 struct RemoteLibrary *RemoteLibraryBase);

#include "remote_protos.h"

/**** Utility Functions ****/

#define APB_PointerAdd(ptr,amt) ((APTR)(((BYTE*)ptr) + amt))

/**** Prevent Execution ****/

LONG __asm LibStart(
    VOID)
{
    return -1;
}

/**** Variables *****/

extern APTR FuncTable[];
extern struct InitDataTable DataTable;
extern struct InitTable InitTab;
extern APTR EndResident;
extern char LibName[];
extern char LibId[];

struct Resident __aligned _LibRomTag = {
    RTC_MATCHWORD,
    &_LibRomTag,
    &EndResident,
    RTF_AUTOINIT,
    LIB_VERSION,
    NT_LIBRARY,
    0,
    &LibName[0],
    &LibId[0],
    &InitTab
};

APTR      EndResident;


char __aligned LibName[] = LIB_NAME ".library";
char __aligned LibId[] = LIB_NAME " " LIB_VER_STR;


struct InitDataTable {

    UWORD     ln_Type_i;
    UWORD     ln_Type_o;
    UWORD     ln_Type_c;
    UBYTE     ln_Name_i;
    UBYTE     ln_Name_o;
    ULONG     ln_Name_c;
    UWORD     lib_Flags_i;
    UWORD     lib_Flags_o;
    UWORD     lib_Flags_c;
    UWORD     lib_Version_i;
    UWORD     lib_Version_o;
    UWORD     lib_Version_c;
    UWORD     lib_Revision_i;
    UWORD     lib_Revision_o;
    UWORD     lib_Revision_c;
    UWORD     lib_IdString_i;
    UBYTE     lib_IdString_o;
    ULONG     lib_IdString_c;
    ULONG     ENDMARK;

} DataTable = {
INITBYTE(OFFSET(Node, ln_Type), NT_LIBRARY),
        0x80, OFFSET(Node, ln_Name), (ULONG) & LibName[0],
        INITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED | LIBF_CHANGED),
        INITWORD(OFFSET(Library, lib_Version), LIB_VERSION),
        INITWORD(OFFSET(Library, lib_Revision), LIB_REVISION), 0x80, OFFSET(Library, lib_IdString), (ULONG) & LibId[0], (ULONG) 0};

struct InitTable {
    ULONG     LibBaseSize;
    APTR     *FuncTable;
    struct InitDataTable *DataTable;
    APTR      InitLibTable;
} InitTab = {
sizeof(struct RemoteLibrary), (APTR *) & FuncTable[0], (struct InitDataTable *) &DataTable, (APTR) InitLib};


APTR      FuncTable[] = {
    _LibOpen,
    _LibClose,
    _LibExpunge,
    ExtFuncLib,

    REM_CreateRemote,
    REM_DestroyRemote,
    REM_GetArgTemplate,
    REM_ConfigureRemote,
    REM_OpenRemote,
    REM_CloseRemote,
    REM_GetSignalBits,
    REM_HandleSignal,
    REM_CanRead,
    REM_Read,
    REM_BytesRead,
    REM_CanWrite,
    REM_Write,
    (APTR) - 1
};

struct ExecBase *SysBase;
struct RemoteLibrary *RemoteBase;

struct RemoteLibrary *__saveds __asm InitLib(
    register __a6 struct Library *sysBase,
    register __a0 BPTR segList,
    register __d0 struct RemoteLibrary *rl)
{
    SysBase = (struct ExecBase *) sysBase;
    RemoteBase = rl;

    rl->r_ExecBase = sysBase;
    rl->r_SegList = segList;

#ifdef REMOTE_LIB_INIT
    if(!REMOTE_LIB_INIT(&rl->r_Data)) {
        return NULL;
    }
#endif

    RemoteBase = rl;

    return rl;
}

struct RemoteLibrary *__saveds __asm _LibOpen(
    register __a6 struct RemoteLibrary *RemoteLibraryBase)
{
    RemoteLibraryBase->r_Lib.lib_OpenCnt++;
    RemoteLibraryBase->r_Lib.lib_Flags &= ~LIBF_DELEXP;

    return RemoteLibraryBase;
}

BPTR __saveds __asm _LibClose(
    register __a6 struct RemoteLibrary * RemoteLibraryBase)
{
    RemoteLibraryBase->r_Lib.lib_OpenCnt--;

    if(RemoteLibraryBase->r_Lib.lib_OpenCnt == 0) {
        if(RemoteLibraryBase->r_Lib.lib_Flags & LIBF_DELEXP) {
            return _LibExpunge(RemoteLibraryBase);
        }
    }
    return NULL;
}

BPTR __saveds __asm _LibExpunge(
    register __a6 struct RemoteLibrary * RemoteLibraryBase)
{
    BPTR      segList;

    if(RemoteLibraryBase->r_Lib.lib_OpenCnt == 0) {

#ifdef REMOTE_LIB_EXPUNGE
        REMOTE_LIB_EXPUNGE(&RemoteLibraryBase->r_Data);
#endif

        segList = RemoteLibraryBase->r_SegList;

        Remove((struct Node *) RemoteLibraryBase);

        return segList;
    }

    RemoteLibraryBase->r_Lib.lib_Flags |= LIBF_DELEXP;

    return NULL;
}

ULONG __saveds __asm ExtFuncLib(
    register __a6 struct RemoteLibrary * RemoteLibraryBase)
{
    return NULL;
}
