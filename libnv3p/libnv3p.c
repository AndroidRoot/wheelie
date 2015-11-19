/* libnv3p.c - Library for Nv3p communications.
 *
 * Copyright (C) 2015 androidroot.mobi
 *
 * This file is part of libnv3p.
 *
 * Wheelie is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * Wheelie is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "shared.h"

#include "libnv3p.h"

/* FIXME: MASSIVE FUCKING KLUDGE. */
static int ICS_MODE = 0;

static void generate_header(uint32_t version, uint32_t type, uint32_t sequence, uint8_t *packet){

    nv3p_header *hdr = (nv3p_header*)packet;
    hdr->version = version;
    hdr->type = type;
    hdr->sequence = sequence;
    return;

}

static uint32_t generate_checksum(uint8_t *packet, uint32_t length){

    uint32_t i;
    uint32_t sum;
    uint8_t *tmp;

    sum = 0;
    tmp = packet;
    for( i = 0; i < length; i++ ){
        sum += *tmp;
        tmp++;
    }

    return sum;

}

static int32_t nv3p_send_ack(nvDevice_t nvdev, uint32_t type, int32_t sequence){

    const uint32_t length = PACKET_HEADER_LEN + PACKET_CHECKSUM_LEN;
    uint8_t packet[PACKET_HEADER_LEN + PACKET_CHECKSUM_LEN];
    uint32_t *checksum = (uint32_t*)&packet[PACKET_HEADER_LEN];
    int ret = 0;


    generate_header(NV3P_VERS, NV3P_ACK, sequence, packet);

    *checksum = generate_checksum(packet, length - PACKET_CHECKSUM_LEN);
    *checksum = ~(*checksum) + 1;

    // TODO: Error handling
    ret = nvusb_send(nvdev, packet, length);

    return 0;

}

static uint32_t remap_command(uint32_t command)
{
    if(ICS_MODE > 0 && command >= NV3P_COMMAND_SENDBL) {
        DEBUGPRINT("Remapped command %d to %d...\n", 
                   command, command + ICS_MODE);
        command += ICS_MODE;
    }

    return command;
}

static int32_t nv3p_get_ack(nvDevice_t nvdev) 
{
    char packet[1024] = {0};
    int packet_len = PACKET_HEADER_LEN;
    
    nv3p_header *header = &packet[0];
    int *recv_checksum, *error, checksum;
    
    /* Receive the header. */
    nvusb_receive(nvdev, &packet[0], PACKET_HEADER_LEN);

    if(header->version != (uint32_t)NV3P_VERS){
        DEBUGPRINT("Nv3p version mismatch\n");
        return -2;
    }
        
    if(header->sequence != nvdev->nvSequence) {
        DEBUGPRINT("Unexpected sequence number: %d, expected: %d\n", 
                   header->sequence, nvdev->nvSequence);
        return -5;
    }

    switch(header->type){
        case NV3P_ACK:
        break;
        case NV3P_NACK:
            error = (int *)&packet[packet_len];
            nvusb_receive(nvdev, &packet[packet_len], 4); // FIXME: Hardcoded 4.
            packet_len += 4;
        break;
        default:
            DEBUGPRINT("Unknown header type received: %d\n", header->type);
            return -4;
        break;
    }
    
    /* Receive the checksum. */
    recv_checksum = &packet[packet_len];
    nvusb_receive(nvdev, &packet[packet_len], PACKET_CHECKSUM_LEN);
    packet_len += PACKET_CHECKSUM_LEN;

    /* Checksum verification of packet */
    checksum = generate_checksum(packet, packet_len - PACKET_CHECKSUM_LEN);

    // usually we do two-complement of checksum.
    // By leaving it out, recv_checksum == -checksum
    checksum += *recv_checksum;

    if(checksum != 0x0){
        DEBUGPRINT("Invalid checksum received: %d\n", *recv_checksum);
        return -1;
    }
    
    if(header->type == NV3P_NACK) {
        return *error;
    }
    
    return 0;
}

#define FILE_BUFFER_SIZE (1024*64)
int nv3p_send_file(nvDevice_t nvdev, char *filename, uint64_t filesize, nv3p_status_callback_t status_callback){
    uint8_t buffer[FILE_BUFFER_SIZE]; // 64KB transfers
    size_t left = (size_t)filesize;
    size_t total_read=0;
    

    FILE *file = fopen(filename, "rb");
    
    while(left > 0)
    {
        size_t wanted = MIN(FILE_BUFFER_SIZE, left);
        size_t read;
        DEBUGPRINT("Reading %ld bytes\n", wanted);
        read = fread(buffer, 1, wanted, file);

        DEBUGPRINT("Got %ld bytes\n", read);
        left -= read;
        total_read += read;
        DEBUGPRINT("Sending bytes to device\n");
        nv3p_send_data(nvdev, buffer, read);
        DEBUGPRINT("Notifying of status\n");
        if(status_callback)
            status_callback(total_read, (size_t)filesize);
    };
    fclose(file);
    return 1;

}

int nv3p_send_buffer(nvDevice_t nvdev, uint8_t *data, int32_t dataLen, 
                     nv3p_status_callback_t status_callback)
{
    size_t left = (size_t)dataLen, wanted;
    
    while(left > 0) {
        wanted = MIN(FILE_BUFFER_SIZE, left);
        nv3p_send_data(nvdev, data, wanted);
        data += wanted;
        left -= wanted;

        if(status_callback)
            status_callback(dataLen - left, (size_t)dataLen);
    }

    return 1;
}

int nv3p_send_data(nvDevice_t nvdev, uint8_t *data, int32_t dataLen) {
    uint8_t *tmp;
    uint8_t packet[PACKET_HEADER_LEN + PACKET_DATA_HEADER_LEN];
    uint32_t length = PACKET_HEADER_LEN + PACKET_DATA_HEADER_LEN;
    uint32_t checksum;
    int ret;

  /* Generate packet header */

    generate_header(NV3P_VERS, NV3P_DATA, nvdev->nvSequence, packet);
    tmp = packet + PACKET_HEADER_LEN;

    *(uint32_t*)tmp = dataLen;
    tmp += PACKET_DATA_HEADER_LEN;

    // Checksum headers + data - since checksum is essentially 
    // just a sum, we can do it seperately
    checksum = generate_checksum(packet, length);
    checksum += generate_checksum(data, dataLen);
    // Take two's complement of the result.
    checksum = ~checksum + 1;

    // Send the three parts seperate because it's easier.
    // Alternative we need to construct a single buffer in memory 
    // and copy stuff around. This also matches usb dumps from nvflash.
    // Send Header, including dataLength
    ret = nvusb_send(nvdev, packet, length);
    if(ret != 0)
        return ret;

    // Send actual data 
    ret = nvusb_send(nvdev, data, dataLen);
    if(ret != 0)
        return ret;

    // Send checksum
    ret = nvusb_send(nvdev, (uint8_t*)&checksum, PACKET_CHECKSUM_LEN);
    if(ret != 0)
        return ret;


    ret = nv3p_get_ack(nvdev);
    if(ret != 0)
        return ret;

    nvdev->nvSequence++;

    return ret;    
}


int nv3p_send_command(nvDevice_t nvdev, uint32_t command, uint8_t *args){

    uint8_t *tmp;
    uint8_t packet[NV3P_MAX_COMMAND_LEN];
    uint32_t length = 0;
    uint32_t checksum;
    nv3p_body *bdy;
    int ret;

    /* Generate packet header */

    generate_header(NV3P_VERS, NV3P_COMMAND, nvdev->nvSequence, packet);

    tmp = packet + PACKET_HEADER_LEN;

    /* Generate packet body */

    bdy = (nv3p_body*)tmp;

    switch(command){
        case NV3P_COMMAND_GETINFO:
        case NV3P_COMMAND_SYNCRONIZE:
            {
                length = 0;
                bdy->length = length;
                bdy->command = command;
                break;
            }
        case NV3P_COMMAND_SENDBCT:
            {
                uint32_t filesize = *(int*) args;
                length = 4;
                bdy->length = length;
                bdy->command = command;
                bdy->bctargs.filesize = filesize;
                break;
            }
        case NV3P_COMMAND_SENDODM:
            {
                uint32_t odmdata = *(int*) args;
                length = 4;
                bdy->length = length;
                bdy->command = command;
                bdy->odmargs.odmdata = odmdata;
                break;
            }
        case NV3P_COMMAND_SENDBL:
            {
            nv3p_blargs *bl = (nv3p_blargs*)args;
            length = 16;
            bdy->length = length;
            bdy->command = command;
            bdy->blargs.filesize = bl->filesize;
            bdy->blargs.loadaddr = bl->loadaddr;
            bdy->blargs.entry = bl->entry;
            break;
            }

        case NV3P_COMMAND_RAWWRITE:
            {
                nv3p_rawargs *raw = (nv3p_rawargs*)args;
                length = 8;
                bdy->length = length;
                bdy->command = command;
                bdy->rawargs.start = raw->start;
                bdy->rawargs.size = raw->size;
                break;
            }
        default:
            return -1;
    }
    
    bdy->command = remap_command(bdy->command);

    length += PACKET_HEADER_LEN;
    length += PACKET_COMMAND_HEADER_LEN;

    checksum = generate_checksum(packet, length);
    checksum = ~checksum + 1;

    tmp = packet + length;

    memcpy(tmp, &checksum, sizeof(checksum));
    tmp += sizeof(checksum);

    length += PACKET_CHECKSUM_LEN;

    
    ret = nvusb_send(nvdev, packet, length);
    if(ret != 0)
        return ret;

    ret = nv3p_get_ack(nvdev);
    if(ret != 0)
        return ret;

    nvdev->nvSequence++;

    return ret;
}

uint8_t nv3p_command_response(nvDevice_t nvdev, uint32_t command, uint8_t *packet){

    uint32_t length = 0;
    const uint32_t hlen = PACKET_HEADER_LEN + PACKET_CHECKSUM_LEN;
    uint32_t recv_checksum = 0, checksum, hdrchecksum;
    uint8_t header[PACKET_HEADER_LEN + PACKET_CHECKSUM_LEN];
//    uint8_t *tmp;
    nv3p_header *hdr;

    switch(command){

        case NV3P_COMMAND_GETINFO:
            // GetInfo appears to return 48 bytes.
            // For now only, the most important ones are mapped out.
            length = NV3P_COMMAND_GETINFO_LEN; 
            break;

        case NV3P_COMMAND_RAWWRITE:
            length = 8;
            break;
        // These "commands" appear to not return any data
        case NV3P_COMMAND_SENDBL:
        case NV3P_COMMAND_SENDBCT:
        case NV3P_COMMAND_SENDODM:
        case NV3P_COMMAND_SYNCRONIZE:
            return 0;
        default:
            return 0x1;
    }

    /* Get return data header */
    nvusb_receive(nvdev, header, hlen);

    hdr = (nv3p_header*)header;

    if(hdr->type != NV3P_DATA)
        return 0x2;
	if(hdr->version != NV3P_VERS)
		printf("Unknown NV3P version: %d\n", hdr->version);

    hdrchecksum = generate_checksum(header, hlen);
    /* Get return data for command */
    nvusb_receive(nvdev, packet, length);

    recv_checksum = generate_checksum(packet, length) + hdrchecksum;

    nvusb_receive(nvdev, (uint8_t*)&checksum, 4);

    if(checksum + recv_checksum != 0x0){
        return 0x3;
    }

    if(hdr->sequence != nvdev->nvRecSequence){
        return 0x4;
    }

    nv3p_send_ack(nvdev, NV3P_ACK, hdr->sequence);

    nvdev->nvRecSequence++;

    return 0;

};

int nv3p_wait_status(nvDevice_t nvdev){
    const uint32_t hlen = PACKET_HEADER_LEN;
    uint8_t header[PACKET_HEADER_LEN];
    nv3p_header *hdr = (nv3p_header*)header;
    uint8_t lencmd[8];
    uint32_t *length = (uint32_t*)lencmd;
    uint32_t *cmd = (uint32_t*)&lencmd[4];
    uint8_t buffer[128]; // We don't technically need 128 bytes - find actual value
    
    nvusb_receive(nvdev, header, hlen);
    nvusb_receive(nvdev, lencmd, 8);

    DEBUGPRINT("Received hdr v%d with type %d\n", hdr->version, hdr->type);
    DEBUGPRINT("Packet with len %d and Command %d\n", *length, *cmd);
    
    if(*length > 0 && *length <= 1024)
    {
        char *string = (char*)buffer;
        uint32_t *status1 = (uint32_t*)&buffer[32];
        uint32_t *status2 = (uint32_t*)&buffer[36];

        nvusb_receive(nvdev, buffer, *length);
    
        if(hdr->type == 5) {
            DEBUGPRINT("NACK PACKET RECEIVED, error: 0x%08x\n", 
                       (uint32_t)*buffer);
            return (uint32_t)*buffer;
        }

        
        DEBUGPRINT("Message: %.*s\n", 32, string);
        DEBUGPRINT("Status1: %u, Status2 = 0x%X\n", *status1, *status2);
    }
    fflush(stdout);
    nv3p_send_ack(nvdev, NV3P_ACK, hdr->sequence);

    nvdev->nvRecSequence++;
    return 0;
};

void nv3p_set_ics_protocol(int flag)
{
    ICS_MODE = flag;
}
