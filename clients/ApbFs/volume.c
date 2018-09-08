
#include "apbfs.h"

#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

#define LOCK_KEY(l) (l ? l->fl_Key : 0)

struct DosList *FS_WaitForDosList(VOID) 
{
    struct DosList *dl;

    while( ! (dl = AttemptLockDosList(LDF_VOLUMES|LDF_WRITE) ) ) {
        Delay(50);
    }

    return dl;
}

struct DeviceList *FS_RemoveDosVolume(STRPTR name)
{
    struct DosList *dl;
    struct DeviceList *v;

    dl = FS_WaitForDosList();

    if( v = (struct DeviceList *)FindDosEntry(dl, name, LDF_VOLUMES) ) {

        RemDosEntry((struct DosList *)v);
    }

    UnLockDosList(LDF_VOLUMES|LDF_WRITE);

    return v;
}

BOOL FS_AddDosVolume(struct DeviceList *vol)
{
    struct DosList *dl;
    BOOL result = FALSE;

    dl = FS_WaitForDosList();

    result = AddDosEntry((struct DosList *)vol);

    UnLockDosList(LDF_VOLUMES|LDF_WRITE); 

    return result;    
}

struct Volume *FS_CreateVolume(UWORD id, STRPTR name)
{
    struct Volume *vol;

    if( vol = (struct Volume *)AllocMem(sizeof(struct Volume), MEMF_ANY|MEMF_CLEAR) ) {

        vol->v_Name = name;
        vol->v_Id = id;

        if( vol->v_Port = CreatePort(NULL, 0) ) {

            if( (vol->v_Vol = FS_RemoveDosVolume(name)) 
             || (vol->v_Vol = (struct DeviceList *)MakeDosEntry(name, DLT_VOLUME) ) ) {

                vol->v_Vol->dl_Task = vol->v_Port;
                vol->v_Vol->dl_DiskType = ID_DOS_DISK;

                if( FS_AddDosVolume(vol->v_Vol) ) { 

                    return vol; 
                }

                FreeDosEntry((struct DosList *)vol->v_Vol);
            }

            DeletePort(vol->v_Port);
        }

        FreeMem(vol, sizeof(struct Volume));
    }

    return NULL;
}

VOID FS_FreeVolume(struct Volume *vol)
{
    if( vol->v_Vol ) {
        FS_RemoveDosVolume(vol->v_Name);
        FreeDosEntry((struct DosList *)vol->v_Vol);
    }

    if( vol->v_Port ) {
        DeletePort(vol->v_Port);
    }

    FreeMem(vol, sizeof(struct Volume));
}


VOID FS_ReplyToDosPacket(struct Volume *vol, struct DosPacket *pkt)
{
    struct Message *msg;
    struct MsgPort *replyPort;
    
    replyPort = pkt->dp_Port;

    msg = pkt->dp_Link;
    msg->mn_Node.ln_Name = (UBYTE *)pkt;
    msg->mn_Node.ln_Succ = NULL;
    msg->mn_Node.ln_Pred = NULL;

    pkt->dp_Port = vol->v_Port;

    PutMsg(replyPort, msg);
}

struct DosPacket *FS_GetDosPacket(struct Message *msg)
{
    return (struct DosPacket *)msg->mn_Node.ln_Name;
}

VOID FS_ActionLocateObject(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *)BADDR(pkt->dp_Arg1);

    UWORD len = *((BYTE *)BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_LOCATE_OBJECT, len);

    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    req->r_Arg1 = LOCK_KEY(l);
    req->r_Arg3 = pkt->dp_Arg3;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionFreeLock(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *)BADDR(pkt->dp_Arg1);
    req = FS_AllocRequest(vol, pkt, ACTION_FREE_LOCK, 0);

    req->r_Arg1 = LOCK_KEY(l);

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionCopyDir(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FileLock *lock = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct FileLock *copy;

    if( ! ( copy = AllocMem(sizeof(struct FileLock), MEMF_ANY | MEMF_CLEAR ) ) ) {

        pkt->dp_Res1 = DOSFALSE;
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;

    } else {
    
        if( lock ) {
            copy->fl_Key = lock->fl_Key;
            copy->fl_Access = lock->fl_Access;
            copy->fl_Task = lock->fl_Task;
            copy->fl_Volume = lock->fl_Volume;
        } else {
            copy->fl_Key = 0;
            copy->fl_Access = SHARED_LOCK;
            copy->fl_Task = vol->v_Port;
            copy->fl_Volume = MKBADDR(vol->v_Vol);
        }

        pkt->dp_Res1 = MKBADDR(copy);
        pkt->dp_Res2 = 0;
    }
}

VOID FS_ActionInfo(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_INFO, 0);

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionOpen(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    UWORD len = *((BYTE *)BADDR(pkt->dp_Arg3)) + 1;
    struct FileLock *lock = (struct FileLock *)BADDR(pkt->dp_Arg2);

    printf("Open >%s< (%d bytes) lock = %ld\n", (STRPTR)BADDR(pkt->dp_Arg3) + 1, len, LOCK_KEY(lock));

    req = FS_AllocRequest(vol, pkt, pkt->dp_Type, len);

    FS_AppendArg(req, 3, len, BADDR(pkt->dp_Arg3));

    req->r_Arg2 = LOCK_KEY(lock);

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionEnd(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_END, 0);

    req->r_Arg1 = pkt->dp_Arg1;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionExamine(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *)BADDR(pkt->dp_Arg1);

    req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_OBJECT, 0);

    req->r_Arg1 = LOCK_KEY(l);
    req->r_Arg2 = 0;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionExamineNext(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *)BADDR(pkt->dp_Arg1);

    struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(pkt->dp_Arg2);

    if( fib->fib_DiskKey < 0 ) {
        fib->fib_DiskKey = 0;
    }

    req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_NEXT, 0);

    req->r_Arg1 = LOCK_KEY(l);
    req->r_Arg2 = fib->fib_DiskKey;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionParent(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *)BADDR(pkt->dp_Arg1);

    req = FS_AllocRequest(vol, pkt, ACTION_PARENT, 0);

    req->r_Arg1 = LOCK_KEY(l);

    FS_EnqueueRequest(fs, req);        
}

VOID FS_ActionRead(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_READ, 0);

    req->r_Arg1 = pkt->dp_Arg1;
    req->r_Arg3 = pkt->dp_Arg3;

    FS_EnqueueRequest(fs, req);        
}

VOID FS_HandlePacket(struct ApbFs *fs, struct Volume *vol, struct DosPacket *pkt)
{
    BOOL reply = FALSE;

    switch( pkt->dp_Type ) {

        case ACTION_LOCATE_OBJECT:
            FS_ActionLocateObject(fs, vol, pkt);
            break;

        case ACTION_FREE_LOCK:
            FS_ActionFreeLock(fs, vol, pkt);
            break;

        case ACTION_COPY_DIR:
            FS_ActionCopyDir(fs, vol, pkt);
            reply = TRUE;
            break;

        case ACTION_IS_FILESYSTEM:
            pkt->dp_Res1 = DOSTRUE;
            pkt->dp_Res2 = 0;
            reply = TRUE;
            break;

        case ACTION_DISK_INFO:
        case ACTION_INFO:
            FS_ActionInfo(fs, vol, pkt);
            break;
        
        case ACTION_SET_PROTECT:
        case ACTION_SET_COMMENT:
        case ACTION_SET_DATE:
            pkt->dp_Res1 = DOSTRUE;
            pkt->dp_Res2 = 0;
            reply = TRUE;
            break;

        case ACTION_FINDINPUT:
        case ACTION_FINDOUTPUT:
        case ACTION_FINDUPDATE:
            FS_ActionOpen(fs, vol, pkt);
            break;

        case ACTION_END:
            FS_ActionEnd(fs, vol, pkt);
            reply = TRUE;
            break;

        case ACTION_EXAMINE_OBJECT:
            FS_ActionExamine(fs, vol, pkt);
            break;

        case ACTION_EXAMINE_NEXT:
            FS_ActionExamineNext(fs, vol, pkt);
            break;

        case ACTION_PARENT:
            FS_ActionParent(fs, vol, pkt);
            break;

        case ACTION_READ:
            FS_ActionRead(fs, vol, pkt);
            break;

        default:        
            printf("%s: Unknown Pkt 0x%lx : Type %d\n", vol->v_Name, pkt, pkt->dp_Type);
            pkt->dp_Res1 = DOSFALSE;
            pkt->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
            reply = TRUE;
            break;
    }

    if( reply ) {
        FS_ReplyToDosPacket(vol, pkt);
    }
}

VOID FS_ReceiveDosMessage(struct ApbFs *fs, struct Volume *vol)
{
    struct Message *msg;
    struct DosPacket *pkt;

    while( msg = GetMsg(vol->v_Port) ) {
        
        pkt = FS_GetDosPacket(msg);
        
        FS_HandlePacket(fs, vol, pkt);
    }
}

VOID FS_ActionLocateObjectReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    struct FileLock *lock;

    if( res->r_Res1 == DOSFALSE ) {
        printf("Failed %d\n", res->r_Res2);
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;            
    } else {

        if( ! ( lock = AllocMem(sizeof(struct FileLock), MEMF_ANY | MEMF_CLEAR ) ) ) {

            pkt->dp_Res1 = DOSFALSE;
            pkt->dp_Res2 = ERROR_NO_FREE_STORE;

        } else {

            lock->fl_Key = res->r_Res2;
            lock->fl_Access = pkt->dp_Arg3;
            lock->fl_Task = vol->v_Port;
            lock->fl_Volume = MKBADDR(vol->v_Vol);

            pkt->dp_Res1 = MKBADDR(lock);
            pkt->dp_Res2 = 0;
        }
    }
}

VOID FS_ActionFreeLockReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    if( pkt->dp_Arg1 ) {
        FreeMem(BADDR(pkt->dp_Arg1), sizeof(struct FileLock));
    }

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionInfoReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    struct InfoData *info;
    LONG* data = (LONG *)(res + 1);

    if( pkt->dp_Type == ACTION_DISK_INFO ) {
        info = BADDR(pkt->dp_Arg1);
    } else {
        info = BADDR(pkt->dp_Arg2);
    }

    info->id_NumSoftErrors = 0;
    info->id_UnitNumber = vol->v_Id;
    info->id_DiskState = ID_VALIDATED;
    info->id_DiskType = ID_INTER_FFS_DISK;
    info->id_VolumeNode = MKBADDR(vol->v_Vol);
    info->id_InUse = DOSFALSE;

    info->id_BytesPerBlock = 512;
    info->id_NumBlocks = *(data++);
    info->id_NumBlocksUsed = *data;

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionOpenReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(pkt->dp_Arg1);

    pkt->dp_Res1 = res->r_Res1;

    if( res->r_Res1 == DOSTRUE ) {

        fh->fh_Arg1 = res->r_Res2;        

    } else {

        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionExamineReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(pkt->dp_Arg2);
    APTR data = res + 1;

    pkt->dp_Res1 = res->r_Res1;

    if( res->r_Res1 == DOSTRUE ) {

        CopyMem(data, fib, 224);

    } else {

        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionReadReply(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res, struct DosPacket *pkt)
{
    APTR src = res + 1;
    APTR dest = (APTR)pkt->dp_Arg2;

    pkt->dp_Res1 = res->r_Res1;
    pkt->dp_Res2 = res->r_Res2;

    if( res->r_Res1 > 0 ) {
        CopyMem(src, dest, res->r_Res1);
    }    
}

VOID FS_ReceiveRemoteMessage(struct ApbFs *fs, struct Volume *vol, struct FsResponse *res)
{
    struct DosPacket *pkt = res->r_Req->r_Pkt;
    BOOL reply = TRUE;

    switch( pkt->dp_Type ) {
    
        case ACTION_LOCATE_OBJECT:
        case ACTION_PARENT:
            FS_ActionLocateObjectReply(fs, vol, res, pkt);
            break;

        case ACTION_FREE_LOCK:
            FS_ActionFreeLockReply(fs, vol, res, pkt);
            break;

        case ACTION_DISK_INFO:
        case ACTION_INFO:
            FS_ActionInfoReply(fs, vol, res, pkt);
            break;

        case ACTION_FINDINPUT:
        case ACTION_FINDOUTPUT:
        case ACTION_FINDUPDATE:
            FS_ActionOpenReply(fs, vol, res, pkt);
            break;

        case ACTION_EXAMINE_OBJECT:
        case ACTION_EXAMINE_NEXT:
            FS_ActionExamineReply(fs, vol, res, pkt);
            break;

        case ACTION_READ:
            FS_ActionReadReply(fs, vol, res, pkt);
            break;

        default:
            pkt->dp_Res1 = res->r_Res1;
            pkt->dp_Res2 = res->r_Res2;            
            break;
    }

    if( reply ) {
        FS_ReplyToDosPacket(vol, pkt);
    }
}

