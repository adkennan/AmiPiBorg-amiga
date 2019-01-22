
#include <exec/types.h>
#include <utility/utility.h>

#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include "lib.h"

/**** Initialization Structs ****/
struct InitTable;
struct InitDataTable;


/**** Library Init ****/
struct AmiPiBorgLibrary *__saveds __asm InitLib(
    register __a6 struct Library *sysBase,
    register __a0 BPTR segList,
    register __d0 struct AmiPiBorgLibrary *amiPiBorgBase);


/**** Library Functions ****/
struct AmiPiBorgLibrary *__saveds __asm _LibOpen(
    register __a6 struct AmiPiBorgLibrary *amiPiBorgBase);

BPTR __saveds __asm _LibClose(
    register __a6 struct AmiPiBorgLibrary *amiPiBorgBase);

BPTR __saveds __asm _LibExpunge(
    register __a6 struct AmiPiBorgLibrary *amiPiBorgBase);

ULONG __saveds __asm _ExtFuncLib(
    register __a6 struct AmiPiBorgLibrary *amiPiBorgBase);

/**** Utility Functions ****/

VOID __saveds FreeLib(
    struct AmiPiBorgLibrary *lib);


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
    APB_LibVersion,
    NT_LIBRARY,
    0,
    &LibName[0],
    &LibId[0],
    &InitTab
};

APTR      EndResident;


char __aligned LibName[] = APB_LibName;
char __aligned LibId[] = APB_LibName " " APB_VersionStr;


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
        INITWORD(OFFSET(Library, lib_Version), APB_LibVersion),
        INITWORD(OFFSET(Library, lib_Revision), APB_LibRevision), 0x80, OFFSET(Library, lib_IdString), (ULONG) & LibId[0], (ULONG) 0};

struct InitTable {
    ULONG     LibBaseSize;
    APTR     *FuncTable;
    struct InitDataTable *DataTable;
    APTR      InitLibTable;
} InitTab = {
sizeof(struct AmiPiBorgLibrary), (APTR *) & FuncTable[0], (struct InitDataTable *) &DataTable, (APTR) InitLib};


APTR      FuncTable[] = {
    _LibOpen,
    _LibClose,
    _LibExpunge,
    _ExtFuncLib,

    APB_CreateContext,
    APB_FreeContext,

    APB_AllocConnection,
    APB_FreeConnection,

    APB_OpenConnection,
    APB_CloseConnection,
    APB_ConnectionState,

    APB_AllocRequest,
    APB_FreeRequest,

    APB_Read,
    APB_Write,
    APB_Abort,

    APB_SrvQuit,
    APB_SrvSetLogLevel,

    APB_AllocMem,
    APB_FreeMem,

    APB_RegisterStat,
    APB_IncrementStat,
    APB_GetStat,

    APB_SetLogLevel,
    APB_LogArgArray,
    APB_ShouldLog,

    APB_TypeRegistered,
    APB_RegisterObjectType,
    APB_AllocObject,
    APB_FreeObject,

    (APTR) - 1
};

struct Library *SysBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *DOSBase = NULL;
struct AmiPiBorgLibrary *AmiPiBorgBase = NULL;

struct AmiPiBorgLibrary *__saveds __asm InitLib(
    register __a6 struct Library *sysBase,
    register __a0 BPTR segList,
    register __d0 struct AmiPiBorgLibrary *amiPiBorgBase)
{
    SysBase = sysBase;

    AmiPiBorgBase = amiPiBorgBase;
    AmiPiBorgBase->l_ExecBase = sysBase;
    AmiPiBorgBase->l_SegList = segList;

    if(AmiPiBorgBase->l_UtilityBase = OpenLibrary(UTILITYNAME, 37)) {
        UtilityBase = AmiPiBorgBase->l_UtilityBase;

        if(AmiPiBorgBase->l_DOSBase = OpenLibrary(DOSNAME, 37)) {
            DOSBase = AmiPiBorgBase->l_DOSBase;

            return AmiPiBorgBase;
        }
    }

    FreeLib(AmiPiBorgBase);
    return NULL;
}

struct AmiPiBorgLibrary *__saveds __asm _LibOpen(
    register __a6 struct AmiPiBorgLibrary *amiPiBorgBase)
{
    amiPiBorgBase->l_Lib.lib_OpenCnt++;
    amiPiBorgBase->l_Lib.lib_Flags &= ~LIBF_DELEXP;

    return amiPiBorgBase;
}

BPTR __saveds __asm _LibClose(
    register __a6 struct AmiPiBorgLibrary * amiPiBorgBase)
{
    amiPiBorgBase->l_Lib.lib_OpenCnt--;

    if(amiPiBorgBase->l_Lib.lib_OpenCnt == 0) {

        if(amiPiBorgBase->l_Lib.lib_Flags & LIBF_DELEXP) {
            return _LibExpunge(amiPiBorgBase);
        }
    }
    return NULL;
}

BPTR __saveds __asm _LibExpunge(
    register __a6 struct AmiPiBorgLibrary * amiPiBorgBase)
{
    BPTR      segList;

    if(amiPiBorgBase->l_Lib.lib_OpenCnt == 0) {

        if(amiPiBorgBase->l_UtilityBase) {
            CloseLibrary(amiPiBorgBase->l_UtilityBase);
        }

        if(amiPiBorgBase->l_DOSBase) {
            CloseLibrary(amiPiBorgBase->l_DOSBase);
        }

        segList = amiPiBorgBase->l_SegList;

        Remove((struct Node *) amiPiBorgBase);

        FreeLib(amiPiBorgBase);

        return segList;
    }

    amiPiBorgBase->l_Lib.lib_Flags |= LIBF_DELEXP;

    return NULL;
}

ULONG __saveds __asm _ExtFuncLib(
    register __a6 struct AmiPiBorgLibrary * amiPiBorgBase)
{
    return NULL;
}

VOID __saveds FreeLib(
    struct AmiPiBorgLibrary * lib)
{
    ULONG     neg, pos, full;
    UBYTE    *negPtr = (UBYTE *) lib;

    neg = lib->l_Lib.lib_NegSize;
    pos = lib->l_Lib.lib_PosSize;
    full = neg + pos;
    negPtr -= neg;

    FreeMem(negPtr, full);
}

VOID     *_XCEXIT = NULL;
