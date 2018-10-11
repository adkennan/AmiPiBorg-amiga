
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

struct DosList *FS_WaitForDosList(
    VOID)
{
    struct DosList *dl;

    while(!(dl = AttemptLockDosList(LDF_VOLUMES | LDF_WRITE))) {
        Delay(50);
    }

    return dl;
}

struct DeviceList *FS_RemoveDosVolume(
    STRPTR name)
{
    struct DosList *dl;
    struct DeviceList *v;

    dl = FS_WaitForDosList();

    if(v = (struct DeviceList *) FindDosEntry(dl, name, LDF_VOLUMES)) {

        RemDosEntry((struct DosList *) v);
    }

    UnLockDosList(LDF_VOLUMES | LDF_WRITE);

    return v;
}

BOOL FS_AddDosVolume(
    struct DeviceList * vol)
{
    struct DosList *dl;
    BOOL      result = FALSE;

    dl = FS_WaitForDosList();

    result = AddDosEntry((struct DosList *) vol);

    UnLockDosList(LDF_VOLUMES | LDF_WRITE);

    return result;
}

struct Volume *FS_CreateVolume(
    UWORD id,
    STRPTR name)
{
    struct Volume *vol;

    if(vol = (struct Volume *) AllocMem(sizeof(struct Volume), MEMF_ANY | MEMF_CLEAR)) {

        vol->v_Name = name;
        vol->v_Id = id;

        if(vol->v_Port = CreatePort(NULL, 0)) {

            if((vol->v_Vol = FS_RemoveDosVolume(name))
               || (vol->v_Vol = (struct DeviceList *) MakeDosEntry(name, DLT_VOLUME))) {

                vol->v_Vol->dl_Task = vol->v_Port;
                vol->v_Vol->dl_DiskType = ID_DOS_DISK;

                if(FS_AddDosVolume(vol->v_Vol)) {

                    return vol;
                }

                FreeDosEntry((struct DosList *) vol->v_Vol);
            }

            DeletePort(vol->v_Port);
        }

        FreeMem(vol, sizeof(struct Volume));
    }

    return NULL;
}

VOID FS_FreeVolume(
    struct Volume * vol)
{
    if(vol->v_Vol) {
        FS_RemoveDosVolume(vol->v_Name);
        FreeDosEntry((struct DosList *) vol->v_Vol);
    }

    if(vol->v_Port) {
        DeletePort(vol->v_Port);
    }

    FreeMem(vol, sizeof(struct Volume));
}


VOID FS_ReplyToDosPacket(
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct Message *msg;
    struct MsgPort *replyPort;

    replyPort = pkt->dp_Port;

    msg = pkt->dp_Link;
    msg->mn_Node.ln_Name = (UBYTE *) pkt;
    msg->mn_Node.ln_Succ = NULL;
    msg->mn_Node.ln_Pred = NULL;

    pkt->dp_Port = vol->v_Port;

    PutMsg(replyPort, msg);
}

struct DosPacket *FS_GetDosPacket(
    struct Message *msg)
{
    return (struct DosPacket *) msg->mn_Node.ln_Name;
}

VOID FS_ActionLocateObject(
    struct ApbFs *fs,
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

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionFreeLock(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    if(key == 0) {

        pkt->dp_Res1 = DOSTRUE;

        FS_ReplyToDosPacket(vol, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_FREE_LOCK, 0);

        req->r_Arg1 = key;

        FS_EnqueueRequest(fs, req);
    }
}

VOID FS_ActionSameLock(
    struct ApbFs *fs,
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

        FS_ReplyToDosPacket(vol, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_SAME_LOCK, 0);

        req->r_Arg1 = key1;
        req->r_Arg2 = key2;

        FS_EnqueueRequest(fs, req);
    }
}

VOID FS_ActionCopyDir(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FileLock *lock = (struct FileLock *) BADDR(pkt->dp_Arg1);
    struct FileLock *copy;

    if(!(copy = AllocMem(sizeof(struct FileLock), MEMF_ANY | MEMF_CLEAR))) {

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
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_INFO, 0);

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionOpen(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg3)) + 1;
    struct FileLock *lock = (struct FileLock *) BADDR(pkt->dp_Arg2);

    req = FS_AllocRequest(vol, pkt, pkt->dp_Type, len);

    FS_AppendArg(req, 3, len, BADDR(pkt->dp_Arg3));

    req->r_Arg2 = LOCK_KEY(lock);

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionEnd(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_END, 0);

    req->r_Arg1 = pkt->dp_Arg1;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionExamine(
    struct ApbFs *fs,
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

        FS_ReplyToDosPacket(vol, pkt);

    } else {

        req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_OBJECT, 0);

        req->r_Arg1 = key;
        req->r_Arg2 = 0;

        FS_EnqueueRequest(fs, req);
    }
}

VOID FS_ActionExamineNext(
    struct ApbFs *fs,
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

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionExamineFH(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileHandle *fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);

    req = FS_AllocRequest(vol, pkt, ACTION_EXAMINE_FH, 0);

    req->r_Arg1 = fh->fh_Arg1;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionParent(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    pkt->dp_Arg3 = SHARED_LOCK;

    req = FS_AllocRequest(vol, pkt, ACTION_PARENT, 0);
    req->r_Arg1 = key;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionRead(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_READ, 0);

    req->r_Arg1 = pkt->dp_Arg1;
    req->r_Arg3 = pkt->dp_Arg3;

    pkt->dp_Res1 = 0;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionWrite(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    LONG      bytesToWrite = pkt->dp_Arg3;
    LONG      packetBytes;
    BYTE     *data = (BYTE *) pkt->dp_Arg2;

    pkt->dp_Res1 = 0;

    while(bytesToWrite > 0) {

        packetBytes = bytesToWrite > 512 ? 512 : bytesToWrite;

        req = FS_AllocRequest(vol, pkt, ACTION_WRITE, packetBytes);
        req->r_Arg1 = pkt->dp_Arg1;
        req->r_Arg3 = pkt->dp_Arg3;

        FS_AppendArg(req, 2, packetBytes, data);

        data += packetBytes;
        bytesToWrite -= packetBytes;

        req->r_Arg3 = bytesToWrite;
        req->r_Arg4 = packetBytes;

        FS_EnqueueRequest(fs, req);
    }
}

VOID FS_ActionSeek(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;

    req = FS_AllocRequest(vol, pkt, ACTION_SEEK, 0);

    req->r_Arg1 = pkt->dp_Arg1;
    req->r_Arg2 = pkt->dp_Arg2;
    req->r_Arg3 = pkt->dp_Arg3;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionCreateDir(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_CREATE_DIR, len);

    req->r_Arg1 = LOCK_KEY(l);
    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionDeleteObject(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    UWORD     len = *((BYTE *) BADDR(pkt->dp_Arg2)) + 1;

    req = FS_AllocRequest(vol, pkt, ACTION_DELETE_OBJECT, len);

    req->r_Arg1 = LOCK_KEY(l);
    FS_AppendArg(req, 2, len, BADDR(pkt->dp_Arg2));

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionRenameObject(
    struct ApbFs *fs,
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

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionParentFH(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg1);
    LONG      key = LOCK_KEY(l);

    pkt->dp_Arg3 = SHARED_LOCK;

    req = FS_AllocRequest(vol, pkt, ACTION_PARENT_FH, 0);
    req->r_Arg1 = key;

    FS_EnqueueRequest(fs, req);
}

VOID FS_ActionFHFromLock(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    struct FsRequest *req;
    struct FileLock *l = (struct FileLock *) BADDR(pkt->dp_Arg2);
    LONG      key = LOCK_KEY(l);

    req = FS_AllocRequest(vol, pkt, ACTION_FH_FROM_LOCK, 0);
    req->r_Arg2 = key;

    FS_EnqueueRequest(fs, req);
}

VOID FS_HandlePacket(
    struct ApbFs *fs,
    struct Volume *vol,
    struct DosPacket *pkt)
{
    BOOL      reply = FALSE;

    switch (pkt->dp_Type) {

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

    case ACTION_SAME_LOCK:
        FS_ActionSameLock(fs, vol, pkt);
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

    case ACTION_EXAMINE_FH:
        FS_ActionExamineFH(fs, vol, pkt);
        break;

    case ACTION_PARENT:
        FS_ActionParent(fs, vol, pkt);
        break;

    case ACTION_READ:
        FS_ActionRead(fs, vol, pkt);
        break;

    case ACTION_WRITE:
        FS_ActionWrite(fs, vol, pkt);
        break;

    case ACTION_SEEK:
        FS_ActionSeek(fs, vol, pkt);
        break;

    case ACTION_PARENT_FH:
        FS_ActionParentFH(fs, vol, pkt);
        break;

    case ACTION_FH_FROM_LOCK:
        FS_ActionFHFromLock(fs, vol, pkt);
        break;

    case ACTION_CREATE_DIR:
        FS_ActionCreateDir(fs, vol, pkt);
        break;

    case ACTION_DELETE_OBJECT:
        FS_ActionDeleteObject(fs, vol, pkt);
        break;

    case ACTION_RENAME_OBJECT:
        FS_ActionRenameObject(fs, vol, pkt);
        break;

    default:
        printf("%s: Unknown Pkt 0x%lx : Type %d\n", vol->v_Name, pkt, pkt->dp_Type);
        pkt->dp_Res1 = DOSFALSE;
        pkt->dp_Res2 = ERROR_ACTION_NOT_KNOWN;
        reply = TRUE;
        break;
    }

    if(reply) {
        FS_ReplyToDosPacket(vol, pkt);
    }
}

VOID FS_ReceiveDosMessage(
    struct ApbFs *fs,
    struct Volume *vol)
{
    struct Message *msg;
    struct DosPacket *pkt;

    while(msg = GetMsg(vol->v_Port)) {

        pkt = FS_GetDosPacket(msg);

        FS_HandlePacket(fs, vol, pkt);
    }
}

VOID FS_ActionLocateObjectReply(
    struct ApbFs *fs,
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileLock *lock;

    if(res->r_Res1 == DOSFALSE) {
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;

    } else if(!(lock = AllocMem(sizeof(struct FileLock), MEMF_ANY | MEMF_CLEAR))) {

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
    struct ApbFs *fs,
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    if(pkt->dp_Arg1) {
        FreeMem(BADDR(pkt->dp_Arg1), sizeof(struct FileLock));
    }

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionInfoReply(
    struct ApbFs *fs,
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
    info->id_DiskType = ID_INTER_FFS_DISK;
    info->id_VolumeNode = MKBADDR(vol->v_Vol);
    info->id_InUse = DOSFALSE;

    info->id_BytesPerBlock = 512;
    info->id_NumBlocks = *(data++);
    info->id_NumBlocksUsed = *data;

    pkt->dp_Res1 = DOSTRUE;
}

VOID FS_ActionOpenReply(
    struct ApbFs *fs,
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
    struct ApbFs *fs,
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
    struct ApbFs *fs,
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
        FS_ReplyToDosPacket(vol, pkt);
        break;
    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        FS_ReplyToDosPacket(vol, pkt);
        break;
    }
}

VOID FS_ActionWriteReply(
    struct ApbFs *fs,
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
        FS_ReplyToDosPacket(vol, pkt);
        break;
    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        FS_ReplyToDosPacket(vol, pkt);
        break;
    }
}

VOID FS_ActionFHFromLockReply(
    struct ApbFs *fs,
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileHandle *fh = (struct FileHandle *) BADDR(pkt->dp_Arg1);

    pkt->dp_Res1 = res->r_Res1;

    if(res->r_Res1 == DOSTRUE) {

        fh->fh_Arg1 = res->r_Res2;

        FreeMem(BADDR(pkt->dp_Arg1), sizeof(struct FileLock));

        pkt->dp_Res2 = 0;
    } else {
        pkt->dp_Res2 = res->r_Res2;
    }
}

VOID FS_ActionCreateDirReply(
    struct ApbFs *fs,
    struct Volume *vol,
    struct FsResponse *res,
    struct DosPacket *pkt)
{
    struct FileLock *l;

    FS_ActionLocateObjectReply(fs, vol, res, pkt);

    if(res->r_Res1 != DOSFALSE) {

        l = BADDR(pkt->dp_Res1);
        l->fl_Access = SHARED_LOCK;
    }
}

VOID FS_ReceiveRemoteMessage(
    struct ApbFs *fs,
    struct Volume *vol,
    struct FsResponse *res)
{
    struct DosPacket *pkt = res->r_Req->r_Pkt;
    BOOL      reply = TRUE;

    switch (pkt->dp_Type) {

    case ACTION_LOCATE_OBJECT:
    case ACTION_PARENT:
    case ACTION_PARENT_FH:
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
    case ACTION_EXAMINE_FH:
        FS_ActionExamineReply(fs, vol, res, pkt);
        break;

    case ACTION_READ:
        FS_ActionReadReply(fs, vol, res, pkt);
        reply = FALSE;
        break;

    case ACTION_WRITE:
        FS_ActionWriteReply(fs, vol, res, pkt);
        reply = FALSE;
        break;

    case ACTION_FH_FROM_LOCK:
        FS_ActionFHFromLockReply(fs, vol, res, pkt);
        break;

    case ACTION_CREATE_DIR:
        FS_ActionCreateDirReply(fs, vol, res, pkt);
        break;

    default:
        pkt->dp_Res1 = res->r_Res1;
        pkt->dp_Res2 = res->r_Res2;
        break;
    }

    if(reply) {
        FS_ReplyToDosPacket(vol, pkt);
    }
}
