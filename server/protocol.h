
#ifndef __APB_PROTOCOL_H__
#define __APB_PROTOCOL_H__

#include <exec/types.h>

#define AMIPIBORG_VERSION 1

#define PACKET_ID 0x416d5069

#define DEFAULT_CONNECTION 0

enum PacketType 
{
	PT_INIT			= 0x00,
	PT_HELLO		= 0x01,
	PT_SHUTDOWN		= 0x02,
	PT_GOODBYE		= 0x03,

	PT_CONNECT		= 0x10,
	PT_CONNECTED	= 0x11,
	PT_DISCONNECT	= 0x12,
	PT_DISCONNECTED	= 0x13,

    PT_DATA         = 0x20,

    PT_RESEND       = 0x22,

    PT_PING         = 0x23,
    PT_PONG         = 0x24,

	PT_ERROR		= 0x30,
    PT_NO_HANDLER   = 0x31,
    PT_NO_CONNECTION= 0x32
};

enum PacketFlag
{
    PF_PAD_BYTE     = 0x01,
    PF_RESEND       = 0x02
};

struct Packet 
{
	ULONG pac_Id;
	UBYTE pac_Type;
	UBYTE pac_Flags;
	UWORD pac_ConnId;
	UWORD pac_PackId;
	UWORD pac_Checksum;
	UWORD pac_Length;
};

struct PacketInit 
{
	struct Packet pi_Pac;
	UWORD pi_Version;
    UWORD pi_MaxPacketSize;
};

struct PacketConnect
{
	struct Packet pc_Pac;
	UWORD pc_HandlerId;
};

struct PacketResponse
{
	struct Packet pr_Pac;
	UWORD pr_PacketId;
};

struct PacketResend
{   
    struct Packet pr_Pac;
    UWORD pr_PacketId;
};

struct PacketHello
{
	UWORD ph_Version;
	UWORD ph_HandlerCount;
};

struct HandlerDesc
{
	UWORD hd_HandlerId;
	BYTE hd_HandlerName[10];
};

UWORD APB_CalculateChecksum(UWORD *data, UWORD length);

ULONG APB_AddToChecksum(ULONG sum, UWORD *data, UWORD length);

UWORD APB_CompleteChecksum(ULONG sum);

#endif // __APB_PROTOCOL_H__
