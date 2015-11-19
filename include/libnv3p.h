/* libnv3p.h - Library for Nv3p communications.
 *
 * Copyright (C) 2015 androidroot.mobi
 *
 * This file is part of libnv3p.
 *
 * libnvusb is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * libnvusb is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <string.h>

#include "libnvusb.h"

#define PACKET_HEADER_LEN       12
#define PACKET_COMMAND_HEADER_LEN     8
#define PACKET_DATA_HEADER_LEN        4
#define PACKET_CHECKSUM_LEN      4
#define PACKET_ACK_LEN        0
#define PACKET_NACK_LEN        4
#define NV3P_VERS		0x1
#define STRING_MAX              32

#define NV3P_COMMAND 0x1
#define NV3P_DATA 0x2
#define NV3P_ACK 0x4
#define NV3P_NACK 0x5

#define NV3P_COMMAND_GETINFO 0x1
#define NV3P_COMMAND_GETINFO_LEN 56
#define NV3P_COMMAND_SENDBCT 0x5
#define NV3P_COMMAND_SENDBL 0x6
#define NV3P_COMMAND_SENDODM 0x7 // Remap?
#define NV3P_COMMAND_STATUS 0x0a
#define NV3P_COMMAND_PARTITION_TABLE 0x13
#define NV3P_COMMAND_SYNCRONIZE 0x19
#define NV3P_COMMAND_RAWWRITE 0x1F

typedef void (*nv3p_status_callback_t) (size_t, size_t);

typedef struct{
    uint32_t start;
    uint32_t size;
} nv3p_rawargs;

typedef struct{
	uint64_t filesize;
	uint32_t loadaddr;
	uint32_t entry;
} nv3p_blargs;

typedef struct{
	uint32_t filesize;
} nv3p_bctargs;

typedef struct{
	uint32_t odmdata;
} nv3p_odmargs;


typedef struct nv3p_header_t{
	uint32_t version;
	uint32_t type;
	uint32_t sequence;
}nv3p_header;


typedef struct nv3p_body_t{
	uint32_t length;
	uint32_t command;
	union
	{
		nv3p_blargs blargs;
		nv3p_bctargs bctargs;
		nv3p_odmargs odmargs;
        nv3p_rawargs rawargs;
	};
} nv3p_body;

// Max command packet length is header + body + checksum
// There might be a longer command, 
// but this is the longest command we've reversed so far.
#define NV3P_MAX_COMMAND_LEN (sizeof(nv3p_header) + sizeof(nv3p_body) + sizeof(int))

typedef struct nv3p_chipid_t{
	uint16_t id;
	uint8_t major;
	uint8_t minor;
}nv3p_chipid;

// TODO: Figure out rest of bootdevice options
#define NV3P_PLATFORM_BOOTDEVICE_EMMC 0x2d

typedef struct nv3p_platinfo_t{
        uint64_t chipuid[2];
        struct nv3p_chipid_t chipid;
        uint32_t sku;
        uint32_t version;
		// Most of these fields are essentially unknowns,
		// but they are printed by nvflash as names that match these.
        uint32_t bootdevice;
        uint32_t opmode;
        uint32_t devstrap;
        uint32_t fuse;
        uint32_t sdramstrap;
		uint32_t devicekeyStatus;
        uint8_t hdcp;
        uint8_t macrovision;
        uint8_t secureboot;
} nv3p_platinfo;

typedef struct{
	struct nv3p_header_t header;
	struct nv3p_body_t body;
	uint32_t checksum;
} nv3p_packet;


int nv3p_send_command(nvDevice_t nvdev, uint32_t command, uint8_t *args);

uint8_t nv3p_command_response(nvDevice_t nvdev, uint32_t command, uint8_t *packet);

int nv3p_wait_status(nvDevice_t nvdev);

int nv3p_send_file(nvDevice_t nvdev, char *filename, uint64_t filesize, nv3p_status_callback_t status_callback);

int nv3p_send_buffer(nvDevice_t nvdev, uint8_t *data, int32_t dataLen, nv3p_status_callback_t status_callback);
int nv3p_send_data(nvDevice_t nvdev, uint8_t *data, int32_t dataLen);
void nv3p_set_ics_protocol(int flag);
