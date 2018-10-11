
#include "apbfs.h"

#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>

struct FsRequest *FS_AllocRequest(
    struct Volume *vol,
    struct DosPacket *pkt,
    UWORD type,
    UWORD length)
{
    struct FsRequest *req = NULL;

    if(req = (struct FsRequest *) AllocMem(sizeof(struct FsRequest) + length, MEMF_ANY)) {

        req->r_Volume = vol;
        req->r_VolId = vol->v_Id;
        req->r_Pkt = pkt;
        req->r_Self = req;
        req->r_Type = type;
        req->r_Length = 0;
        req->r_Arg1 = 0;
        req->r_Arg2 = 0;
        req->r_Arg3 = 0;
        req->r_Arg4 = 0;
    }

    return req;
}

VOID FS_AppendArg(
    struct FsRequest * req,
    UWORD arg,
    UWORD length,
    APTR data)
{
    APTR      dest = ((BYTE *) (req + 1)) + req->r_Length;

    CopyMem(data, dest, length);

    switch (arg) {
    case 1:
        req->r_Arg1 = req->r_Length;
        break;
    case 2:
        req->r_Arg2 = req->r_Length;
        break;
    case 3:
        req->r_Arg3 = req->r_Length;
        break;
    case 4:
        req->r_Arg4 = req->r_Length;
        break;
    default:
        break;
    }

    req->r_Length += length;
}

VOID FS_FreeRequest(
    struct FsRequest *req)
{
    FreeMem(req, sizeof(struct FsRequest) + req->r_Length);
}
