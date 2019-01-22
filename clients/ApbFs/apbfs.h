
#ifndef __APBFS_H__
#define __APBFS_H__

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/ports.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include "amipiborg.h"

#define MAX_NAME_LEN 31
#define MAX_WRITE_LEN 512

enum {
    OT_VOLUME = 0x1122,
    OT_REQUEST_MIN = 0x1123,
    OT_REQUEST_MAX = 0x1124,
    OT_LOCK = 0x1125
};

enum DeviceState
{
    ST_STARTING,
    ST_MOUNTED,
    ST_NOT_CONNECTED,
    ST_CONNECTED,
    ST_STOPPING
};

struct ApbFs
{
    APTR fs_Ctx;
    APTR fs_Conn;
    APTR fs_MP;
    struct MsgPort *fs_RemotePort;
	struct APBRequest *fs_Reader;
	struct APBRequest *fs_Writer;
	APTR fs_ReadBuf;
    struct MinList fs_Vols;
	struct MinList fs_Reqs;
	struct MinList fs_SentReqs;
    UWORD fs_Status;
    ULONG fs_VolSigBits;
	BOOL fs_WriteReady;
};

struct Volume
{
    struct MinNode v_Node;
    struct ApbFs *v_Fs;
    STRPTR v_Name;
	struct MsgPort *v_Port;
    struct DeviceList *v_Vol;
    UWORD v_Id;
	WORD v_LockCount;
    BOOL v_Mounted;
};

struct FsRequest
{
	struct MinNode r_Node;
	struct Volume *r_Volume;
	struct DosPacket *r_Pkt;
    struct FsRequest *r_Self;
	ULONG r_Arg1;
	ULONG r_Arg2;
	ULONG r_Arg3;
	ULONG r_Arg4;
	UWORD r_VolId;
	UWORD r_Type;
	UWORD r_Length;
};

struct FsResponse
{
	struct FsRequest *r_Req;
	ULONG r_Res1;
	ULONG r_Res2;
	UWORD r_Length;
};

struct FsFileLock
{
    struct FileLock fl_RealLock;
    UWORD fl_Magic;
};

#define MOUNT_VOLUME_KEY 0xFFFFFFFF
#define UNMOUNT_VOLUME_KEY 0xFFFFFFFE

struct FsMountVolume
{
    ULONG mv_Key;
    UWORD mv_VolId;
	BYTE mv_Name;
};

struct FsUnmountVolume 
{
	ULONG uv_Key;
	UWORD uv_VolId;
};

struct FsRequest *FS_AllocRequest(struct Volume *vol, struct DosPacket *pkt, UWORD type, UWORD length);

VOID FS_AppendArg(struct FsRequest *req, UWORD arg, UWORD length, APTR data);

VOID FS_FreeRequest(struct FsRequest *req);

struct Volume *FS_CreateVolume(struct ApbFs *fs, UWORD id, STRPTR name);

VOID FS_FreeVolume(struct Volume *vol);

BOOL FS_MountVolume(struct Volume *vol);

VOID FS_UnmountVolume(struct Volume *vol);

VOID FS_ReceiveDosMessage(struct Volume *vol);

VOID FS_ReceiveRemoteMessage(struct Volume *vol, struct FsResponse *res);

VOID FS_EnqueueRequest(struct ApbFs *fs, struct FsRequest *req);

struct ApbFs *FS_CreateDevice(VOID);

VOID FS_FreeDevice(struct ApbFs *fs);

VOID FS_Run(struct ApbFs *fs);


#endif // __APBFS_H__