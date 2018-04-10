
#ifndef __APB_H__
#define __APB_H__

#include <exec/types.h>
#include <exec/ports.h>

#define AMIPIBORG_VERSION 1

#define PACKET_ID 0x416d5069

#define DEFAULT_CONNECTION 0

typedef enum 
{
	PT_INIT			= 0x00,
	PT_HELLO		= 0x01,
	PT_SHUTDOWN		= 0x02,
	PT_GOODBYE		= 0x03,

	PT_CONNECT		= 0x10,
	PT_CONNECTED	= 0x11,
	PT_DISCONNECT	= 0x12,
	PT_DISCONNECTED	= 0x13,

	PT_DATA			= 0x20,
	PT_ACK			= 0x21,
	PT_RESEND		= 0x22,
	
	PT_ERROR		= 0x30,
} PacketType;

typedef enum 
{
	PF_ACK_REQ		= 0x00,
    PF_PAD_BYTE     = 0x01
} PacketFlag;

struct Packet 
{
	ULONG pac_Id;
	UBYTE pac_Type;
	UBYTE pac_Flags;
	UWORD pac_ConnId;
	UWORD pac_PackId;
	ULONG pac_Checksum;
	UWORD pac_Length;
};

struct PacketInit 
{
	struct Packet pi_Pac;
	UWORD pi_Version;
};

struct PacketHello
{
	struct Packet ph_Pac;
	UWORD ph_Version;
	UWORD ph_HandlerCount;
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

struct HandlerDesc
{
	UWORD hd_HandlerId;
	UBYTE hd_HandlerName;
};


#endif // __APB_H__
