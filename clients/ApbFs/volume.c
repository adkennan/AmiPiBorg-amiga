
#include "apbfs.h"

#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <string.h>

#define LOCK_KEY(l) (l ? l->fl_Key : 0)

#define DEV_NAME_SIZE 10

struct DosList *FS_WaitForDosList(
    LONG type)
{
    struct DosList *dl;

    while(!(dl = AttemptLockDosList(type | LDF_WRITE))) {
        Delay(50);
    }

    return dl;
}

struct DeviceList *FS_RemoveDosEntry(
    STRPTR name,
    LONG type)
{
    struct DosList *dl;
    struct DeviceList *v;

    dl = FS_WaitForDosList(type);

    if(v = (struct DeviceList *) FindDosEntry(dl, name, type)) {

        RemDosEntry((struct DosList *) v);
    }

    UnLockDosList(type | LDF_WRITE);

    return v;
}

BOOL FS_AddDosEntry(
    struct DosList * obj,
    LONG type)
{
    BOOL      result;

    FS_WaitForDosList(type);

    result = AddDosEntry(obj);

    UnLockDosList(type | LDF_WRITE);

    return result;
}

struct FileLock *FS_AllocLock(
    struct Volume *vol)
{

    struct FsFileLock *internalLock;

    if(!(internalLock = (struct FsFileLock *) APB_AllocObject(vol->v_Fs->fs_Ctx, OT_LOCK))) {

        return NULL;
    }

    internalLock->fl_Magic = (UWORD) ((ULONG) internalLock) & 0xffff;

    LOG3(vol->v_Fs->fs_Ctx, LOG_TRACE, "Alloc lock %x, magic %d, count %d", internalLock, internalLock->fl_Magic, vol->v_LockCount);

    vol->v_LockCount++;

    return (struct FileLock *) internalLock;
}

VOID FS_FreeLock(
    struct Volume *vol,
    struct FileLock *lock)
{

    struct FsFileLock *internalLock;

    if(lock == NULL) {

        LOG1(vol->v_Fs->fs_Ctx, LOG_TRACE, "Free NULL lock, count %d", vol->v_LockCount);
        return;
    }

    internalLock = (struct FsFileLock *) lock;

    LOG3(vol->v_Fs->fs_Ctx, LOG_TRACE, "Free lock %x, magic %d, count %d", internalLock, internalLock->fl_Magic, vol->v_LockCount);

    if(internalLock->fl_Magic != (UWORD) ((ULONG) internalLock) & 0xffff) {
        LOG1(vol->v_Fs->fs_Ctx, LOG_ERROR, "Lock %x doesn't belong to me", lock);
    } else {

        APB_FreeObject(vol->v_Fs->fs_Ctx, OT_LOCK, internalLock);

        vol->v_LockCount--;
        if(vol->v_LockCount < 0) {
            LOG1(vol->v_Fs->fs_Ctx, LOG_ERROR, "Lock count less than 0: %d", vol->v_LockCount);
        }
    }
}

BOOL FS_MountVolume(
    struct Volume *vol)
{
    if((vol->v_Vol = (struct DeviceList *) MakeDosEntry(vol->v_Name, DLT_VOLUME))) {

        vol->v_Vol->dl_Task = vol->v_Port;
        vol->v_Vol->dl_DiskType = ID_DOS_DISK;

        if(FS_AddDosEntry((struct DosList *) vol->v_Vol, LDF_VOLUMES)) {

            vol->v_LockCount = 0;
            vol->v_Mounted = TRUE;

            return TRUE;
        }
    }

    FreeDosEntry((struct DosList *) vol->v_Vol);
}

VOID FS_UnmountVolume(
    struct Volume *vol)
{
    if(!vol->v_Mounted) {
        return;
    }

    if(vol->v_Vol) {
        FS_RemoveDosEntry(vol->v_Name, LDF_VOLUMES);
        FreeDosEntry((struct DosList *) vol->v_Vol);
        vol->v_Vol = NULL;
    }

    vol->v_Mounted = FALSE;
}

BOOL FS_CreateDosDevice(
    struct Volume *vol)
{

    if(vol->v_DevName = APB_AllocMem(vol->v_Fs->fs_Ctx, DEV_NAME_SIZE)) {

        sprintf(vol->v_DevName, "APB%d", vol->v_Id);

        if((vol->v_Dev = (struct DevInfo *) MakeDosEntry(vol->v_DevName, DLT_DEVICE))) {

            vol->v_Dev->dvi_Task = vol->v_Port;

            if(FS_AddDosEntry((struct DosList *) vol->v_Dev, LDF_DEVICES)) {

                return TRUE;
            }
        }

        FreeDosEntry((struct DosList *) vol->v_Vol);
    }

    APB_FreeMem(vol->v_Fs->fs_Ctx, vol->v_DevName, DEV_NAME_SIZE);
}

VOID FS_FreeDosDevice(
    struct Volume *vol)
{

    if(vol->v_Dev) {
        FS_RemoveDosEntry(vol->v_DevName, LDF_DEVICES);
        FreeDosEntry((struct DosList *) vol->v_Dev);
    }

    if(vol->v_DevName) {
        APB_FreeMem(vol->v_Fs->fs_Ctx, vol->v_DevName, DEV_NAME_SIZE);
    }

}

struct Volume *FS_CreateVolume(
    struct ApbFs *fs,
    UWORD id,
    STRPTR name)
{
    struct Volume *vol;

    if(vol = (struct Volume *) APB_AllocObject(fs->fs_Ctx, OT_VOLUME)) {

        vol->v_Fs = fs;
        vol->v_Name = (STRPTR) APB_PointerAdd(vol, sizeof(struct Volume));
        vol->v_Id = id;

        strncpy(vol->v_Name, name, MAX_NAME_LEN);

        if(vol->v_Port = CreatePort(NULL, 0)) {

            if(FS_CreateDosDevice(vol)) {

                if(FS_MountVolume(vol)) {
                    return vol;
                }

                FS_FreeDosDevice(vol);
            }

            DeletePort(vol->v_Port);
        }

        APB_FreeObject(fs->fs_Ctx, OT_VOLUME, vol);
    }

    return NULL;
}

VOID FS_FreeVolume(
    struct Volume * vol)
{
    if(vol->v_Vol) {
        FS_UnmountVolume(vol);
        FreeDosEntry((struct DosList *) vol->v_Vol);
    }

    FS_FreeDosDevice(vol);

    if(vol->v_Port) {
        DeletePort(vol->v_Port);
    }

    APB_FreeObject(vol->v_Fs->fs_Ctx, OT_VOLUME, vol);
}

VOID FS_ReplyToDosPacket(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct Message *msg;
    struct MsgPort *replyPort;
    struct FsRequest *req;

    replyPort = pkt->dp_Port;

    msg = pkt->dp_Link;
    msg->mn_Node.ln_Name = (UBYTE *) pkt;
    msg->mn_Node.ln_Succ = NULL;
    msg->mn_Node.ln_Pred = NULL;

    pkt->dp_Port = vol->v_Port;

    PutMsg(replyPort, msg);

    if(res && (req = res->r_Req)) {

        FS_FreeRequest(req);
    }
}

struct DosPacket *FS_GetDosPacket(
    struct Message *msg)
{
    return (struct DosPacket *) msg->mn_Node.ln_Name;
}

VOID FS_ActionLocateObject(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);

    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_LOCATE_OBJECT, len);

    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    req->r_Arg1 = LOCK_KEY(l);
    req->r_Arg3 = pkt->dp_Arg3;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionFreeLock(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    if(key == 0) {

        pkt->dp_Res1 = DOSTRUE;

        FS_ReplyToDosPacket(vol, NULL, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_FREE_LOCK, 0);

        req->r_Arg1 = key;

        FS_EnqueueRequest(vol->v_Fs, req);
    }
}

VOID FS_ActionSameLock(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l1 = (struct FileLock *) BADDR(pkt->dp_Arg1);
    struct FileLock *l2 = (struct FileLock *) BADDR(pkt->dp_Arg2);
    LONG      key1 = LOCK_KEY(l1);
    LONG      key2 = LOCK_KEY(l2);

    if(key1 == key2) {

        pkt->dp_Res1 = DOSTRUE;

        FS_ReplyToDosPacket(vol, NULL, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_SAME_LOCK, 0);

        req->r_Arg1 = key1;
        req->r_Arg2 = key2;

        FS_EnqueueRequest(vol->v_Fs, req);
    }
}

VOID FS_ActionCopyDir(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FileLock *lock = BADDR(pkt->dp_Arg1);
    struct FileLock *copy;

    if(!(copy = FS_AllocLock(vol))) {

        pkt->dp_Res1 = DOSFALSE;
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;

    } else {

        if(lock && lock->fl_Key) {
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

VOID FS_ActionInfo(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct InfoData *info;

    if(vol->v_Mounted) {

        req = FS_AllocRequest(vol, pkt, ACTION_INFO, 0);

        FS_EnqueueRequest(vol->v_Fs, req);

    } else {

        if(pkt->dp_Type == ACTION_DISK_INFO) {
            info = BADDR(pkt->dp_Arg1);
        } else {
            info = BADDR(pkt->dp_Arg2);
        }

        info->id_NumSoftErrors = 0;
        info->id_UnitNumber = vol->v_Id;
        info->id_DiskState = ID_NO_DISK_PRESENT;
        info->id_DiskType = ID_NO_DISK_PRESENT;
        info->id_VolumeNode = NULL;
        info->id_InUse = DOSFALSE;

        info->id_BytesPerBlock = 0;
        info->id_NumBlocks = 0;
        info->id_NumBlocksUsed = 0;

        pkt->dp_Res1 = DOSTRUE;
    }
}

VOID FS_ActionOpen(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg3)) + 1;
    struct FileLock *lock = (struct FileLock *) BADDR(pkt->dp_Arg2);

    req = FS_AllocRequest(vol, pkt, pkt->dp_Type, len);

    FS_AppendArg(req, 3, len, BADDR(pkt->dp_Arg3));

    req->r_Arg2 = LOCK_KEY(lock);

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionEnd(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_END, 0);

    req->r_Arg1 = pkt->dp_Arg1;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionExamine(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);
    struct FileInfoBlock *fib;

    if(key == 0) {

        fib = (struct FileInfoBlock *) BADDR(pkt->dp_Arg2);
        fib->fib_DiskKey = 0;
        fib->fib_DirEntryType = 2;
        fib->fib_FileName[0] = (BYTE) strlen(vol->v_Name);
        CopyMem(vol->v_Name, &fib->fib_FileName[1], fib->fib_FileName[0]);
        fib->fib_Protection = 0;
        fib->fib_EntryType = 2;
        fib->fib_Size = 0;
        fib->fib_NumBlocks = 0;
        DateStamp(&fib->fib_Date);
        fib->fib_Comment[0] = '\0';
        fib->fib_Comment[1] = '\0';

        FS_ReplyToDosPacket(vol, NULL, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_OBJECT, 0);

        req->r_Arg1 = key;
        req->r_Arg2 = 0;

        FS_EnqueueRequest(vol->v_Fs, req);
    }
}

VOID FS_ActionExamineNext(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);

    struct FileInfoBlock *fib = (struct FileInfoBlock *) BADDR(pkt->dp_Arg2);

    if(fib->fib_DiskKey < 0) {
        fib->fib_DiskKey = 0;
    }

    req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_NEXT, 0);

    req->r_Arg1 = LOCK_KEY(l);
    req->r_Arg2 = fib->fib_DiskKey;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionExamineFH(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileHandle *fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);

    req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_FH, 0);

    req->r_Arg1 = fh->fh_Arg1;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionParent(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    pkt->dp_Arg3 = SHARED_LOCK;

    req = FS_AllocRequest(vol, pkt, ACTION_PARENT, 0);
    req->r_Arg1 = key;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionRead(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_READ, 0);

    req->r_Arg1 = pkt->dp_Arg1;
    req->r_Arg3 = pkt->dp_Arg3;

    pkt->dp_Res1 = 0;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionWrite(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    LONG      bytesToWrite = pkt->dp_Arg3;
    LONG      packetBytes;
    BYTE     *data = (BYTE *) pkt->dp_Arg2;

    pkt->dp_Res1 = 0;

    while(bytesToWrite > 0) {

        packetBytes = bytesToWrite > MAX_WRITE_LEN ? MAX_WRITE_LEN : bytesToWrite;

        req = FS_AllocRequest(vol, pkt, ACTION_WRITE, packetBytes);
        req->r_Arg1 = pkt->dp_Arg1;
        req->r_Arg3 = pkt->dp_Arg3;

        FS_AppendArg(req, 2, packetBytes, data);

        data += packetBytes;
        bytesToWrite -= packetBytes;

        req->r_Arg3 = bytesToWrite;
        req->r_Arg4 = packetBytes;

        FS_EnqueueRequest(vol->v_Fs, req);
    }
}

VOID FS_ActionSeek(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_SEEK, 0);

    req->r_Arg1 = pkt->dp_Arg1;
    req->r_Arg2 = pkt->dp_Arg2;
    req->r_Arg3 = pkt->dp_Arg3;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionCreateDir(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_CREATE_DIR, len);

    req->r_Arg1 = LOCK_KEY(l);
    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionDeleteObject(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_DELETE_OBJECT, len);

    req->r_Arg1 = LOCK_KEY(l);
    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionRenameObject(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l1 = (struct FileLock *) BADDR(pkt->dp_Arg1);
    UWORD     len1 = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;
    struct FileLock *l2 = (struct FileLock *) BADDR(pkt->dp_Arg3);
    UWORD     len2 = *((BYTE *) BADDR(pkt->dp_Arg4)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_RENAME_OBJECT, len1 + len2);

    req->r_Arg1 = LOCK_KEY(l1);
    FS_AppendArg(req, 2, len1, BADDR(pkt->dp_Arg2));
    req->r_Arg3 = LOCK_KEY(l2);
    FS_AppendArg(req, 4, len2, BADDR(pkt->dp_Arg4));

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionParentFH(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    pkt->dp_Arg3 = SHARED_LOCK;

    req = FS_AllocRequest(vol, pkt, ACTION_PARENT_FH, 0);
    req->r_Arg1 = key;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_ActionFHFromLock(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg2);
    LONG      key = LOCK_KEY(l);

    req = FS_AllocRequest(vol, pkt, ACTION_FH_FROM_LOCK, 0);
    req->r_Arg2 = key;

    FS_EnqueueRequest(vol->v_Fs, req);
}

VOID FS_HandlePacket(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    BOOL      reply = FALSE;

    if(vol->v_Mounted) {

        switch (pkt->dp_Type) {

        case ACTION_LOCATE_OBJECT:
            FS_ActionLocateObject(vol, pkt);
            break;

        case ACTION_FREE_LOCK:
            FS_ActionFreeLock(vol, pkt);
            break;

        case ACTION_COPY_DIR:
            FS_ActionCopyDir(vol, pkt);
            reply = TRUE;
            break;

        case ACTION_SAME_LOCK:
            FS_ActionSameLock(vol, pkt);
            break;

        case ACTION_IS_FILESYSTEM:
            pkt->dp_Res1 = DOSTRUE;
            pkt->dp_Res2 = 0;
            reply = TRUE;
            break;

        case ACTION_DISK_INFO:
        case ACTION_INFO:
            FS_ActionInfo(vol, pkt);
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
        case ACTION_FLUSH:
            FS_ActionOpen(vol, pkt);
            break;

        case ACTION_END:
            FS_ActionEnd(vol, pkt);
            reply = TRUE;
            break;

        case ACTION_EXAMINE_OBJECT:
            FS_ActionExamine(vol, pkt);
            break;

        case ACTION_EXAMINE_NEXT:
            FS_ActionExamineNext(vol, pkt);
            break;

        case ACTION_EXAMINE_FH:
            FS_ActionExamineFH(vol, pkt);
            break;

        case ACTION_PARENT:
            FS_ActionParent(vol, pkt);
            break;

        case ACTION_READ:
            FS_ActionRead(vol, pkt);
            break;

        case ACTION_WRITE:
            FS_ActionWrite(vol, pkt);
            break;

        case ACTION_SEEK:
            FS_ActionSeek(vol, pkt);
            break;

        case ACTION_PARENT_FH:
            FS_ActionParentFH(vol, pkt);
            break;

        case ACTION_FH_FROM_LOCK:
            FS_ActionFHFromLock(vol, pkt);
            break;

        case ACTION_CREATE_DIR:
            FS_ActionCreateDir(vol, pkt);
            break;

        case ACTION_DELETE_OBJECT:
            FS_ActionDeleteObject(vol, pkt);
            break;

        case ACTION_RENAME_OBJECT:
            FS_ActionRenameObject(vol, pkt);
            break;

        case ACTION_CURRENT_VOLUME:
            pkt->dp_Res1 = MKBADDR(vol->v_Vol);
            pkt->dp_Res2 = 0;
            reply = TRUE;
            break;

        default:
            LOG3(vol->v_Fs->fs_Ctx, LOG_ERROR, "%s: Unknown Pkt 0x%x : Type %d", vol->v_Name, pkt, (ULONG) pkt->dp_Type);
            pkt->dp_Res1 = DOSFALSE;
            pkt->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
            reply = TRUE;
            break;
        }


    } else {

        switch (pkt->dp_Type) {
        case ACTION_FREE_LOCK:
            FS_ActionFreeLock(vol, pkt);
            break;
        case ACTION_DISK_INFO:
            FS_ActionInfo(vol, pkt);
            reply = TRUE;
            break;
        default:
            LOG2(vol->v_Fs->fs_Ctx, LOG_INFO, "%s: Action %d while unmounted", vol->v_Name, pkt->dp_Type);
            pkt->dp_Res1 = DOSFALSE;
            pkt->dp_Res2 = ERROR_DEVICE_NOT_MOUNTED;
            reply = TRUE;
            break;
        }
    }

    if(reply) {
        FS_ReplyToDosPacket(vol, NULL, pkt);
    }
}

VOID FS_ReceiveDosMessage(
    struct Volume *vol)
{
    struct Message *msg;
    struct DosPacket *pkt;

    while(msg = GetMsg(vol->v_Port)) {

        pkt = FS_GetDosPacket(msg);

        FS_HandlePacket(vol, pkt);
    }
}

VOID FS_ActionLocateObjectReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileLock *lock;

    if(res->r_Res1 == DOSFALSE) {
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;

    } else if(!(lock = FS_AllocLock(vol))) {

        pkt->dp_Res1 = DOSFALSE;
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;

    } else {

        lock->fl_Key = res->r_Res1;
        lock->fl_Access = pkt->dp_Arg3;
        lock->fl_Task = vol->v_Port;
        lock->fl_Volume = MKBADDR(vol->v_Vol);

        pkt->dp_Res1 = MKBADDR(lock);
        pkt->dp_Res2 = 0;
    }
}

VOID FS_ActionFreeLockReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    if(pkt->dp_Arg1) {

        FS_FreeLock(vol, BADDR(pkt->dp_Arg1));
    }

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionInfoReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct InfoData *info;
    LONG     *data = (LONG *) (res + 1);

    if(pkt->dp_Type == ACTION_DISK_INFO) {
        info = BADDR(pkt->dp_Arg1);
    } else {
        info = BADDR(pkt->dp_Arg2);
    }

    info->id_NumSoftErrors = 0;
    info->id_UnitNumber = vol->v_Id;
    info->id_DiskState = ID_VALIDATED;
    info->id_DiskType = 0x41504200;     // 'APB\0'
    info->id_VolumeNode = MKBADDR(vol->v_Vol);
    info->id_InUse = DOSFALSE;

    info->id_BytesPerBlock = *(data++);
    info->id_NumBlocks = *(data++);
    info->id_NumBlocksUsed = *data;

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionOpenReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileHandle *fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);

    pkt->dp_Res1 = res->r_Res1;

    if(res->r_Res1 == DOSTRUE) {

        fh->fh_Arg1 = res->r_Res2;

    } else {

        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionExamineReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *) BADDR(pkt->dp_Arg2);
    APTR      data = res + 1;

    pkt->dp_Res1 = res->r_Res1;

    if(res->r_Res1 == DOSTRUE) {

        CopyMem(data, fib, 224);

    } else {

        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionReadReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    // Some applications assume that if a read does not fill up the buffer
    // in pkt->dp_Arg2 there must be no more data.
    // To get around this we'll use dp_Res1 to store a current offset into
    // the buffer and only reply to the packet when the buffer is full or
    // the entire file has been read.
    // When the handler has no more data to send it will set r_Res2 to -1.

    APTR      src = ((BYTE *) res) + sizeof(struct FsResponse);
    APTR      dest = (APTR) ((BYTE *) pkt->dp_Arg2 + pkt->dp_Res1);

    if(res->r_Res1 > 0) {

        CopyMem(src, dest, res->r_Res1);
        pkt->dp_Res1 += res->r_Res1;
        pkt->dp_Res2 = 0;
    }

    switch (res->r_Res2) {
    case 0:
        break;
    case -1:
        FS_ReplyToDosPacket(vol, res, pkt);
        break;
    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        FS_ReplyToDosPacket(vol, res, pkt);
        break;
    }
}

VOID FS_ActionWriteReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    // The write operation may be broken into multiple requests. Don't
    // respond to the DOS packet until all the bytes have been written.
    // When the handler has written all the data it will set r_Res2 to -1.

    if(res->r_Res1 > 0) {

        pkt->dp_Res1 += res->r_Res1;
        pkt->dp_Res2 = 0;
    }

    switch (res->r_Res2) {
    case 0:
        break;
    case -1:
        FS_ReplyToDosPacket(vol, res, pkt);
        break;
    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        FS_ReplyToDosPacket(vol, res, pkt);
        break;
    }
}

VOID FS_ActionFHFromLockReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileHandle *fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);

    pkt->dp_Res1 = res->r_Res1;

    if(res->r_Res1 == DOSTRUE) {

        fh->fh_Arg1 = res->r_Res2;

        FS_FreeLock(vol, BADDR(pkt->dp_Arg1));

        pkt->dp_Res2 = 0;
    } else {
        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionCreateDirReply(
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileLock *l;

    FS_ActionLocateObjectReply(vol, res, pkt);

    if(res->r_Res1 != DOSFALSE) {

        l = BADDR(pkt->dp_Res1);
        l->fl_Access = SHARED_LOCK;
    }
}

VOID FS_ReceiveRemoteMessage(
    struct Volume *vol,
    struct FsResponse *res)
{
    struct DosPacket *pkt = res->r_Req->r_Pkt;
    BOOL      reply = TRUE;

    switch (pkt->dp_Type) {

    case ACTION_LOCATE_OBJECT:
    case ACTION_PARENT:
    case ACTION_PARENT_FH:
        FS_ActionLocateObjectReply(vol, res, pkt);
        break;

    case ACTION_FREE_LOCK:
        FS_ActionFreeLockReply(vol, res, pkt);
        break;

    case ACTION_DISK_INFO:
    case ACTION_INFO:
        FS_ActionInfoReply(vol, res, pkt);
        break;

    case ACTION_FINDINPUT:
    case ACTION_FINDOUTPUT:
    case ACTION_FINDUPDATE:
        FS_ActionOpenReply(vol, res, pkt);
        break;

    case ACTION_EXAMINE_OBJECT:
    case ACTION_EXAMINE_NEXT:
    case ACTION_EXAMINE_FH:
        FS_ActionExamineReply(vol, res, pkt);
        break;

    case ACTION_READ:
        FS_ActionReadReply(vol, res, pkt);
        reply = FALSE;
        break;

    case ACTION_WRITE:
        FS_ActionWriteReply(vol, res, pkt);
        reply = FALSE;
        break;

    case ACTION_FH_FROM_LOCK:
        FS_ActionFHFromLockReply(vol, res, pkt);
        break;

    case ACTION_CREATE_DIR:
        FS_ActionCreateDirReply(vol, res, pkt);
        break;

    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        break;
    }

    if(reply) {
        FS_ReplyToDosPacket(vol, res, pkt);
    }
}
