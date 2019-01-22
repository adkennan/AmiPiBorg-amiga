
#include "apbfs.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stddef.h>
#include <stdio.h>

#define VOL_LIST(fs) ((struct List *)&(fs->fs_Vols))
#define REQ_LIST(fs) ((struct List *)&(fs->fs_Reqs))
#define SENTREQ_LIST(fs) ((struct List *)&(fs->fs_SentReqs))

#define READ_BUF_SIZE 4096

#define PORT_BIT(port) (1 << port->mp_SigBit)

struct ApbFs *FS_CreateDevice(
    VOID)
{
    struct ApbFs *fs;
    APTR      mp;

    if(mp = CreatePool(MEMF_ANY, 4096, 2048)) {

        if(fs = AllocPooled(mp, sizeof(struct ApbFs))) {

            fs->fs_MP = mp;

            if(fs->fs_Ctx = APB_CreateContext(mp, Output(), LOG_TRACE)) {

                if(fs->fs_ReadBuf = AllocPooled(mp, READ_BUF_SIZE)) {

                    if(fs->fs_RemotePort = CreatePort(NULL, 0)) {

                        if(fs->fs_Conn = APB_AllocConnection(fs->fs_Ctx, fs->fs_RemotePort, 4)) {

                            if(fs->fs_Reader = APB_AllocRequest(fs->fs_Conn)) {

                                if(fs->fs_Writer = APB_AllocRequest(fs->fs_Conn)) {

                                    if(APB_OpenConnection(fs->fs_Conn)) {

                                        LOG0(fs->fs_Ctx, LOG_INFO, "Hello From ApbFs");

                                        fs->fs_Status = ST_STARTING;

                                        NewList(VOL_LIST(fs));

                                        NewList(REQ_LIST(fs));

                                        NewList(SENTREQ_LIST(fs));

                                        fs->fs_WriteReady = TRUE;
                                        fs->fs_Reader->r_Data = fs->fs_ReadBuf;
                                        fs->fs_Reader->r_Length = READ_BUF_SIZE;

                                        APB_RegisterObjectType(fs->fs_Ctx, OT_VOLUME, "Volume", sizeof(struct Volume) + MAX_NAME_LEN + 1, 1, 8);
                                        APB_RegisterObjectType(fs->fs_Ctx, OT_REQUEST_MIN, "Request (Min)", sizeof(struct FsRequest), 1, 100);
                                        APB_RegisterObjectType(fs->fs_Ctx, OT_REQUEST_MAX, "Request (Max)", sizeof(struct FsRequest) + MAX_WRITE_LEN, 1, 100);
                                        APB_RegisterObjectType(fs->fs_Ctx, OT_LOCK, "Lock", sizeof(struct FsFileLock), 4, 100);

                                        APB_Read(fs->fs_Reader);

                                        return fs;
                                    }
                                    APB_FreeRequest(fs->fs_Writer);
                                }
                                APB_FreeRequest(fs->fs_Reader);
                            }
                            APB_FreeConnection(fs->fs_Conn);
                        }
                        DeletePort(fs->fs_RemotePort);
                    }
                    FreePooled(mp, fs->fs_ReadBuf, READ_BUF_SIZE);
                }
                APB_FreeContext(fs->fs_Ctx);
            }
            FreePooled(mp, fs, sizeof(struct ApbFs));
        }

        DeletePool(mp);
    }

    return NULL;
}

VOID FS_FreeDevice(
    struct ApbFs * fs)
{
    APTR      mp = fs->fs_MP;

    if(fs->fs_Reader) {
        APB_FreeRequest(fs->fs_Reader);
    }

    if(fs->fs_Writer) {
        APB_FreeRequest(fs->fs_Writer);
    }

    if(fs->fs_ReadBuf) {
        FreePooled(mp, fs->fs_ReadBuf, READ_BUF_SIZE);
    }

    if(fs->fs_Conn) {

        LOG0(fs->fs_Ctx, LOG_INFO, "Goodbye From ApbFs");

        APB_CloseConnection(fs->fs_Conn);

        APB_FreeConnection(fs->fs_Conn);
    }

    if(fs->fs_RemotePort) {
        DeletePort(fs->fs_RemotePort);
    }

    while(!IsListEmpty(VOL_LIST(fs))) {
        FS_FreeVolume((struct Volume *) RemHead(VOL_LIST(fs)));
    }

    if(fs->fs_Ctx) {
        APB_FreeContext(fs->fs_Ctx);
    }

    FreePooled(mp, fs, sizeof(struct ApbFs));

    DeletePool(mp);
}

VOID FS_SendReq(
    struct ApbFs *fs)
{
    struct FsRequest *req;

    if(IsListEmpty(REQ_LIST(fs))) {
        return;
    }

    req = (struct FsRequest *) RemHead(REQ_LIST(fs));

    fs->fs_Writer->r_Data = ((BYTE *) req) + offsetof(struct FsRequest,
                                                      r_Self);
    fs->fs_Writer->r_Length = sizeof(struct FsRequest) + req->r_Length - offsetof(struct FsRequest,
                                                                                  r_Self);

    APB_Write(fs->fs_Writer);

    AddTail(SENTREQ_LIST(fs), (struct Node *) req);

    fs->fs_WriteReady = FALSE;
}

VOID FS_HandleMountVolume(
    struct ApbFs *fs,
    struct FsMountVolume *mv)
{
    struct Volume *vol;

    for(vol = (struct Volume *) VOL_LIST(fs)->lh_Head; vol->v_Node.mln_Succ; vol = (struct Volume *) vol->v_Node.mln_Succ) {
        if(vol->v_Id == mv->mv_VolId) {

            LOG2(fs->fs_Ctx, LOG_INFO, "Re-mount volume %d - %s\n", vol->v_Id, vol->v_Name);

            FS_MountVolume(vol);

            return;
        }
    }

    if(vol = FS_CreateVolume(fs, mv->mv_VolId, &mv->mv_Name)) {

        LOG2(fs->fs_Ctx, LOG_INFO, "Mount volume %d - %s\n", mv->mv_VolId, &mv->mv_Name);

        AddTail(VOL_LIST(fs), (struct Node *) vol);
        fs->fs_VolSigBits |= PORT_BIT(vol->v_Port);
    }
}

VOID FS_HandleUnmountVolume(
    struct ApbFs *fs,
    struct FsUnmountVolume *uv)
{
    struct Volume *vol;

    for(vol = (struct Volume *) VOL_LIST(fs)->lh_Head; vol->v_Node.mln_Succ; vol = (struct Volume *) vol->v_Node.mln_Succ) {
        if(vol->v_Id == uv->uv_VolId) {

            LOG2(fs->fs_Ctx, LOG_INFO, "Unmount volume %d - %s\n", vol->v_Id, vol->v_Name);

            if(vol->v_LockCount > 0) {

                LOG2(fs->fs_Ctx, LOG_ERROR, "Volume %s has %d outstanding locks", vol->v_Name, vol->v_LockCount);
            }

            FS_UnmountVolume(vol);

            break;
        }
    }
}

VOID FS_ReceiveResponse(
    struct ApbFs *fs)
{
    struct FsResponse *res = (struct FsResponse *) fs->fs_Reader->r_Data;

    switch ((ULONG) res->r_Req) {
    case MOUNT_VOLUME_KEY:
        FS_HandleMountVolume(fs, (struct FsMountVolume *) res);
        break;

    case UNMOUNT_VOLUME_KEY:
        FS_HandleUnmountVolume(fs, (struct FsUnmountVolume *) res);
        break;

    default:
        Remove((struct Node *) res->r_Req);

        FS_ReceiveRemoteMessage(res->r_Req->r_Volume, res);
        break;
    }

    APB_Read(fs->fs_Reader);
}

VOID FS_ReceiveFromRemote(
    struct ApbFs *fs)
{
    struct Message *msg;

    while(msg = GetMsg(fs->fs_RemotePort)) {

        if(msg == (struct Message *) fs->fs_Reader) {

            if(fs->fs_Reader->r_State == APB_RS_OK) {

                FS_ReceiveResponse(fs);

            } else {

                printf("Error: %d\n", fs->fs_Reader->r_State);
            }

        } else if(msg == (struct Message *) fs->fs_Writer) {

            fs->fs_WriteReady = TRUE;
            FS_SendReq(fs);

        } else {
            ReplyMsg(msg);
        }
    }
}

BOOL FS_CanQuit(
    struct ApbFs *fs)
{
    struct Volume *vol;

    for(vol = (struct Volume *) VOL_LIST(fs)->lh_Head; vol->v_Node.mln_Succ; vol = (struct Volume *) vol->v_Node.mln_Succ) {
        if(vol->v_LockCount > 0) {
            LOG2(fs->fs_Ctx, LOG_ERROR, "Cannot quit, Volume %s has %d outstanding locks.", vol->v_Name, vol->v_LockCount);
            return FALSE;
        }
    }

    return TRUE;
}

VOID FS_Run(
    struct ApbFs * fs)
{
    ULONG     sig;
    struct Volume *vol;

    while(fs->fs_Status != ST_STOPPING) {

        sig = Wait(PORT_BIT(fs->fs_RemotePort) | fs->fs_VolSigBits | SIGBREAKF_CTRL_C);

        for(vol = (struct Volume *) VOL_LIST(fs)->lh_Head; vol->v_Node.mln_Succ; vol = (struct Volume *) vol->v_Node.mln_Succ) {

            if(sig & PORT_BIT(vol->v_Port)) {
                FS_ReceiveDosMessage(vol);
                break;
            }
        }

        if(sig & PORT_BIT(fs->fs_RemotePort)) {
            FS_ReceiveFromRemote(fs);
        }

        if(sig & SIGBREAKF_CTRL_C) {
            LOG0(fs->fs_Ctx, LOG_INFO, "Break!");
            if(FS_CanQuit(fs)) {
                fs->fs_Status = ST_STOPPING;
            }
        }
    }
}


VOID FS_EnqueueRequest(
    struct ApbFs *fs,
    struct FsRequest *req)
{
    AddTail(REQ_LIST(fs), (struct Node *) req);

    if(fs->fs_WriteReady) {
        FS_SendReq(fs);
    }
}
