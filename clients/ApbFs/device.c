
#include "apbfs.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stddef.h>
#include <stdio.h>

#define VOL_LIST(device) ((struct List *)&(device->fs_Vols))
#define REQ_LIST(device) ((struct List *)&(device->fs_Reqs))
#define SENTREQ_LIST(device) ((struct List *)&(device->fs_SentReqs))

#define READ_BUF_SIZE 4096

#define PORT_BIT(port) (1 << port->mp_SigBit)

struct ApbFs *FS_CreateDevice(
    VOID)
{
    struct ApbFs *device;

    if(device = AllocMem(sizeof(struct ApbFs), MEMF_ANY | MEMF_CLEAR)) {

        if(device->fs_ReadBuf = AllocMem(READ_BUF_SIZE, MEMF_ANY)) {

            if(device->fs_RemotePort = CreatePort(NULL, 0)) {

                if(device->fs_Conn = APB_AllocConnection(device->fs_RemotePort, 4, NULL)) {

                    if(device->fs_Reader = APB_AllocRequest(device->fs_Conn)) {

                        if(device->fs_Writer = APB_AllocRequest(device->fs_Conn)) {

                            if(APB_OpenConnection(device->fs_Conn)) {

                                APB_Log(device->fs_Conn, 5, "Hello From ApbFs");

                                device->fs_Status = ST_STARTING;

                                NewList(VOL_LIST(device));

                                NewList(REQ_LIST(device));

                                NewList(SENTREQ_LIST(device));

                                device->fs_WriteReady = TRUE;

                                device->fs_Reader->r_Data = device->fs_ReadBuf;
                                device->fs_Reader->r_Length = READ_BUF_SIZE;

                                APB_Read(device->fs_Reader);

                                return device;
                            }
                            APB_FreeRequest(device->fs_Writer);
                        }
                        APB_FreeRequest(device->fs_Reader);
                    }
                    APB_FreeConnection(device->fs_Conn);
                }
                DeletePort(device->fs_RemotePort);
            }
            FreeMem(device->fs_ReadBuf, READ_BUF_SIZE);
        }
        FreeMem(device, sizeof(struct ApbFs));
    }

    return NULL;
}

VOID FS_FreeDevice(
    struct ApbFs * device)
{
    if(device->fs_Reader) {
        APB_FreeRequest(device->fs_Reader);
    }

    if(device->fs_Writer) {
        APB_FreeRequest(device->fs_Writer);
    }

    if(device->fs_ReadBuf) {
        FreeMem(device->fs_ReadBuf, READ_BUF_SIZE);
    }

    if(device->fs_Conn) {

        APB_Log(device->fs_Conn, 5, "Goodbye From ApbFs");

        APB_CloseConnection(device->fs_Conn);

        APB_FreeConnection(device->fs_Conn);
    }

    if(device->fs_RemotePort) {
        DeletePort(device->fs_RemotePort);
    }

    while(!IsListEmpty(VOL_LIST(device))) {
        FS_FreeVolume((struct Volume *) RemHead(VOL_LIST(device)));
    }

    FreeMem(device, sizeof(struct ApbFs));
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

VOID FS_CreateNewVolume(
    struct ApbFs *fs,
    struct FsNewVolume *nv)
{
    struct Volume *vol;
    STRPTR    name = (STRPTR) (nv + 1);

    printf("Create volume %d - %s\n", nv->nv_VolId, name);

    if(vol = FS_CreateVolume(nv->nv_VolId, name)) {
        AddTail(VOL_LIST(fs), (struct Node *) vol);
        fs->fs_VolSigBits |= PORT_BIT(vol->v_Port);
    }
}

VOID FS_ReceiveResponse(
    struct ApbFs *fs)
{
    struct FsResponse *res = (struct FsResponse *) fs->fs_Reader->r_Data;

    if((ULONG) res->r_Req == NEW_VOLUME_KEY) {

        FS_CreateNewVolume(fs, (struct FsNewVolume *) res);

    } else {

        Remove((struct Node *) res->r_Req);

        FS_ReceiveRemoteMessage(fs, res->r_Req->r_Volume, res);
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


VOID FS_Run(
    struct ApbFs *device)
{
    ULONG     sig;
    struct Volume *vol;

    while(device->fs_Status != ST_STOPPING) {

        sig = Wait(PORT_BIT(device->fs_RemotePort) | device->fs_VolSigBits | SIGBREAKF_CTRL_C);

        for(vol = (struct Volume *) VOL_LIST(device)->lh_Head; vol->v_Node.mln_Succ; vol = (struct Volume *) vol->v_Node.mln_Succ) {

            if(sig & PORT_BIT(vol->v_Port)) {
                FS_ReceiveDosMessage(device, vol);
                break;
            }
        }

        if(sig & PORT_BIT(device->fs_RemotePort)) {
            FS_ReceiveFromRemote(device);
        }

        if(sig & SIGBREAKF_CTRL_C) {
            APB_Log(device->fs_Conn, 5, "Break!");
            device->fs_Status = ST_STOPPING;
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
