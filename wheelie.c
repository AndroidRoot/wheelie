/* wheelie.c - Preflight for nvflash
 *
 * Copyright (C) 2015 androidroot.mobi
 *
 * This file is part of Wheelie.
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
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdint.h>
#include <sys/stat.h>

/* Includes for wheelie functions and payloads */

#include "config.h"
#include "shared.h"
#include "wheelie.h"
#include "libnv3p.h"
#include "libnvfblob.h"

#define VENDOR_ID 0x955
#define TEGRA2_PRODUCT_ID 0x7820
#define TEGRA33_PRODUCT_ID 0x7330
#define TEGRA30_PRODUCT_ID 0x7030

static void usage() {
    printf("Usage: wheelie --blob <blob file>\n\n");
    printf("Argument                Description\n\n");
    printf("-v or --version         Display version information\n");
    printf("-h or --help            Display this menu\n");
    printf("-b or --blob            Specify blob file to use\n");
    printf("-U or --unbrick         Flash bricksafe image\n");
    exit(0);
}

void file_transfer_cb(size_t transferred, size_t total){
    size_t percent = (size_t)(((float)transferred/(float)total)*100.0);
    printf("\rSending file: %lu %%", (unsigned long)percent);
    if(transferred == total)
        printf("\n");
}

int main(int argc, char * const* argv) {
    int32_t device = -1;
    int32_t ret;
    uint64_t chipuid_rcm;
    nvDevice_t nvdev;
    uint8_t *buf ,*blptr;
    size_t bufsize, bricksafe_size;
    uint32_t rawsize;
    char *blobfile = NULL;
    char *bricksafe = NULL;
    nv3p_blargs blargs;
    nv3p_rawargs rawargs;
    uint64_t responsedata = -1;
    uint32_t response = (uint32_t)responsedata;
	int32_t c;
	uint8_t r_data[NV3P_COMMAND_GETINFO_LEN];
	int32_t status;
    int32_t recovery_mode = 0;
    int32_t upload_bricksafe = 0;
	nv3p_platinfo *platform;
	char* cpu_type;
    nvfblob_file_t blob;

    setbuf(stdout, NULL);

    printf("Wheelie %s - Preflight for nvflash.\n", WHEELIE_VERSION);
    printf("Copyright (c) 2011-2012 androidroot.mobi\n");
    printf("========================================\n\n");

    while (1)
    {
        static struct option long_options[] = {
            {"version",     no_argument,        0, 'v'},
            {"help",        no_argument,        0, 'h'},
            {"blob",        required_argument,  0, 'b'},
            {"recovery",    required_argument,  0, 'r'},
            {"unbrick",     required_argument,  0, 'U'},
            {0, 0, 0, 0}
        };

        int32_t option_index = 0;

		c = -1;
        c = getopt_long (argc, argv, "vhb:rU:",
                        long_options, &option_index);

        if (c == -1) break;

        switch(c) {

            case 'v':
                printf("Wheelie version is %s\n", WHEELIE_VERSION);
                nvusb_versions();
                printf("Nv3P library version is %s and is ABI version %s\n\n", NV3P_VERSION, NV3P_ABI);
                exit(0);

            case 'h':
                usage();
                break;

            case 'b':
                blobfile = optarg;
                break;

            case 'r':
                recovery_mode = 1;
                break;

            case 'U':
               /* Unbricking implies recovery */
               recovery_mode = 1;
               upload_bricksafe = 1;
               bricksafe = optarg;
               break;

            case '?':
                if (isprint(optopt))
                    usage();
                else
                    usage();
                return 1;

            default:
            abort();
        }
    }    
    
    if(!blobfile) {
        printf("[-] You must specify --blob.\n");
        exit(0);
    }

    if(!(blob = nvfblob_open(blobfile))) {
        printf("[-] Failed to open blob file: %s\n", blobfile);
        exit(0);
    }
    
    printf("Waiting for device in APX mode...\n");
    while(device < 0) {
        device = nvusb_detect_device(VENDOR_ID, TEGRA2_PRODUCT_ID, &nvdev);
        usleep(250000); 
        if(device >= 0) break;
        device = nvusb_detect_device(VENDOR_ID, TEGRA33_PRODUCT_ID, &nvdev);
        usleep(250000); 
        if(device >= 0) break;
        device = nvusb_detect_device(VENDOR_ID, TEGRA30_PRODUCT_ID, &nvdev);
        usleep(250000); 
    }

    // Receive chipuid - first thing sent after poweron in APX mode.
    // We have to receive  the exact number of bytes RCM sends
    // Sadly, we have no clue what the second value means.
    nvusb_receive_unbuffered(nvdev, (uint8_t*)&chipuid_rcm, sizeof(uint64_t));
    printf("[=] Chip UID: 0x%llx\n", (unsigned long long)chipuid_rcm);
    DEBUGPRINT("Asking for Rcm version\n");
    
    if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_RCMVER, &buf, &bufsize)){
        printf("[-] Failed to read RCMVERS from blob file.\n");
        exit(0);
    }

    if(nvusb_send(nvdev, buf, bufsize) != 0) {

        printf("[-] RCM_Send communication failure.\n");
        exit(0);
    }
    
    nvfblob_alloc_free(&buf);

    if(nvusb_receive_unbuffered(nvdev, (uint8_t*)&responsedata, 8) != 0) {

        printf("[-] RCM_Receive communication failure.\n");
        exit(0);
    }
    response = (uint32_t)responsedata;

    if(response != 0x20001 && response != 0x30001) {
        if(response == 0x4) {
            printf("[-] Incorrect BLOB selected. nverror: 0x%x.\n", response);
            exit(0);
        } else {
            printf("[-] Unknown error or incorrect RCM protocol version. nverror: 0x%x.\n", response);
            exit(0);
        }
    }

    printf("[=] RCM Version: 0x%x\n\n", response);
    
    if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_RCMDLEXEC, 
                           &buf, &bufsize)) {
        printf("[-] Failed to read RCMDLEXEC message from blob file.\n");
        exit(0);
    }

    DEBUGPRINT("Sending buf\n");
    if(nvusb_send(nvdev, (uint8_t*)buf, bufsize) != 0) {

        printf("[-] RCM_Send communication failure.\n");
        exit(0);
    }

    nvfblob_alloc_free(&buf);

    DEBUGPRINT("Receiving response\n");
    if(nvusb_receive_unbuffered(nvdev, (uint8_t*)&responsedata, sizeof(uint64_t)) != 0) {

        printf("[-] RCM_Receive communication failure.\n");
        exit(0);
    }
    response = (uint32_t)responsedata;
    DEBUGPRINT("Got response 0x%x\n", response);
    if(response != 0x0) {

        printf("[-] Miniloader upload failed with error: 0x%x.\n", response);
        exit(0);
    }

    DEBUGPRINT("Sending getinfo command\n");
    ret = nv3p_send_command(nvdev, NV3P_COMMAND_GETINFO, NULL);
    if(ret != 0){
        printf("[-] Error %d sending command\n", ret);
        exit(0);
    }

    
    ret = nv3p_command_response(nvdev, NV3P_COMMAND_GETINFO, r_data);
    if(ret != 0){
        printf("[-] Error %d receiving response from command\n", ret);
        exit(0);
    }
    status = nv3p_wait_status(nvdev);
    platform = (nv3p_platinfo*)r_data;

    if(platform->chipid.id == 0x20){
        cpu_type = "Tegra 2";
        blargs.loadaddr = 0x108000;
        blargs.entry = blargs.loadaddr;
    }else if(platform->chipid.id == 0x30){
        cpu_type = "Tegra 3";
        blargs.loadaddr = 0x80108000;
        blargs.entry = blargs.loadaddr;
        //nv3p_set_ics_protocol(1);
    }else{
        cpu_type = "Unknown";
    }
	
/*    if(chipuid_rcm != platform->chipuid){
        printf("[-] Data returned was invalid\n");
        exit(0);
    }*/

    if(!strcmp(cpu_type, "Unknown")){
        printf("[-] CPU type 0x%x is unknown\n", platform->chipid.id);
        exit(0);
    }

    printf("[=] CPU Model: %s\n", cpu_type);
    if(platform->chipid.id == 0x20 || recovery_mode)
    {
        if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_EXT_WHEELIE_BCTR, 
                               &buf, &bufsize)) {
            printf("[-] Failed to read (recovery) BCT from blob file.\n");
            exit(0);
        }

        // Send BCT
        printf("[+] Sending BCT\n");
        ret = nv3p_send_command(nvdev, NV3P_COMMAND_SENDBCT,(uint8_t*)&bufsize);
        DEBUGPRINT("nv3p_send_command (BCT) returned %d\n", ret);
        DEBUGPRINT("Sending BCT with size %lu\n", bufsize);
        ret = nv3p_send_buffer(nvdev, buf, bufsize, &file_transfer_cb);
        status = nv3p_wait_status(nvdev);
        DEBUGPRINT("Sent BCT with result: %d\n", status);

        nvfblob_alloc_free(&buf);
        
        if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_EXT_WHEELIE_ODMDATA, 
                               &buf, &bufsize)) {
            printf("[-] Failed to read ODMDATA from blob file.\n");
            exit(0);
        }
        
        printf("[+] Sending ODMData 0x%X\n", (uint32_t)*buf);
        nv3p_send_command(nvdev, NV3P_COMMAND_SENDODM, (uint8_t*)&buf);
        DEBUGPRINT("Waiting for result...\n");
        status = nv3p_wait_status(nvdev);
        DEBUGPRINT("Sent ODM with result: %d\n", status);

        nvfblob_alloc_free(&buf);
    }
    
    if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_EXT_WHEELIE_BL, &blptr, 
                           &blargs.filesize)) {
        printf("[-] Failed to read BL from blob file.\n");
        exit(0);
    }

    printf("[+] Sending bootloader...\n");
    DEBUGPRINT("Sending uploadbl command\n");
    ret = nv3p_send_command(nvdev, NV3P_COMMAND_SENDBL, (uint8_t*)&blargs);
    if(ret != 0){
        printf("[-] Error %d sending command\n", ret);
        exit(0);
    }
    status = nv3p_wait_status(nvdev);
    
    if(platform->chipid.id == 0x30) {
		DEBUGPRINT("Reading blhash\n");
        if(!nvfblob_alloc_read(blob, NVFBLOB_TYPE_BLHASH, &buf, 
                               &bufsize)) {
            printf("[-] Failed to read BLHash from blob file.\n");
            exit(0);
        }
		DEBUGPRINT("Sending BLHash\n");
        ret = nv3p_send_buffer(nvdev, buf, bufsize, NULL);
        if(ret != 1) {
            printf("[-] Failed sending BLHash\n");
            exit(0);
        }
        
        nvfblob_alloc_free(&buf);
    }


    DEBUGPRINT("Sending bootloader file (size=%llu)\n", blargs.filesize);

    ret = nv3p_send_buffer(nvdev, blptr, blargs.filesize, &file_transfer_cb);
    status = nv3p_wait_status(nvdev);
    DEBUGPRINT("Sent bootloader with result: %d\n", status);
    
    nvfblob_alloc_free(&blptr);

    /* Receive sequence needs to be reset after changing from miniloader
     * to bootloader nv3p server. For some reason they seem to keep seperate
     * packet counters */
    nvdev->nvRecSequence = 1;

    if(upload_bricksafe == 1)
    {
        DEBUGPRINT("Sending Bricksafe image\n")
        printf("[+] Sending bricksafe image\n");
        if(filetest(bricksafe, &bufsize) == 0)
        {
            printf("[-] Error %s not found\n", bricksafe);
            exit(0);
        } else if(bufsize == 0) {
            printf("[-] Error: Read bricksafe image as 0 bytes!\n");
            exit(0);
        }

        bricksafe_size = bufsize;

        /* We need to tell the bootloader where to start, and how many
         * sectors the raw write should be.*/
        rawargs.start = 0;
        rawargs.size = 2944;

        ret = nv3p_send_command(nvdev, NV3P_COMMAND_RAWWRITE, (uint8_t*)&rawargs);
        if(ret != 0)
        {
            printf("[-] Error %d sending command\n", ret);
            exit(0);
        }

        ret = nv3p_command_response(nvdev, NV3P_COMMAND_RAWWRITE, (uint8_t*)&rawsize);
        if(ret != 0)
        {
            printf("[-] Error %d receiving response from command\n", ret);
            exit(0);
        }
        
        if(rawsize > (uint32_t)bricksafe_size)
        {
            printf("[-] Error %d is smaller than required: %d\n", 
                   (uint32_t)bricksafe_size, rawsize);
            exit(0);
        }

        ret = nv3p_send_file(nvdev, bricksafe, rawsize, &file_transfer_cb);
        if(ret != 1)
        {
            printf("[-] Error %d sending file\n", ret);
            exit(0);
        }

        status = nv3p_wait_status(nvdev);

        printf("[!] Done - restart your device now\n");
    } else {
        DEBUGPRINT("Sending sync command\n");
        ret = nv3p_send_command(nvdev, NV3P_COMMAND_SYNCRONIZE, NULL);
        if(ret < 0) {
            printf("[-] Error %d sending command\n", ret);
            exit(0);
        } else if(ret > 0) {
            printf("[*] Warning - unexpected response to sync command: %d\n", 
                   ret);
        }
        
        status = nv3p_wait_status(nvdev);
        DEBUGPRINT("Send sync with result: %d\n", status);
    
        printf("[!] Done - your device should now be ready for nvflash\n");
    }

    nvusb_device_close(&nvdev);
    exit(0);
}
