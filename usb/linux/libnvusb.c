/* libnvusb.c - Library for nvusb functions
*
* Copyright (C) 2015 androidroot.mobi
*
* This file is part of libnvusb.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "config.h"
#include "shared.h"
#include "libnvusb.h"

static int32_t getEndpoints(struct libusb_config_descriptor *config, uint8_t *ep_in, uint8_t *ep_out){
	int32_t i;
	int32_t ret=-2;
	for(i=0; i<config->interface[0].altsetting[0].bNumEndpoints; i++){
		if((config->interface[0].altsetting[0].endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN){
			*ep_in = config->interface[0].altsetting[0].endpoint[i].bEndpointAddress;
			ret++;
		}else{
			*ep_out = config->interface[0].altsetting[0].endpoint[i].bEndpointAddress;
			ret++;
		}
	}
	return ret;
}

int nvusb_detect_device(int vendid, int32_t prodid, nvDevice_t* pnvdev){
	// declarations
	int err;
	libusb_device **list;
	libusb_device *dev = NULL;
	nvDevice_t nvdev;
	ssize_t cnt; 
	ssize_t i = 0;
	int ret = 0;
	struct libusb_config_descriptor *config = NULL;
	
	// Code
	*pnvdev = NULL;	
	libusb_init(NULL);
	nvdev = (nvDevice_t) malloc(sizeof(struct nvDevice));
	memset(nvdev, 0, sizeof(struct nvDevice));
	libusb_set_debug(NULL, 3);
	cnt = libusb_get_device_list(NULL, &list);
	if (cnt < 0)
		ret = -2;
	for (i = 0; i < cnt; i++) {
		libusb_device *device = list[i];
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(device, &desc);
		if (desc.idVendor == vendid && desc.idProduct == prodid) {
			dev = device;
			ret = 0;
			break;
		}else{
			ret = -1;
		}
	}

	if(ret == 0){
		err = libusb_open(dev, &nvdev->nvDeviceID);
		if(err)
			return -3;
		
		err = libusb_claim_interface(nvdev->nvDeviceID, 0);
		if(err)
			return -4;
		libusb_free_device_list(list, 1);
		nvdev->nvDeviceIDGet = libusb_get_device(nvdev->nvDeviceID);

		err = libusb_get_active_config_descriptor(nvdev->nvDeviceIDGet, &config);
		if(err)
			return -5;
		getEndpoints(config, &nvdev->nvDeviceEndpointIn, &nvdev->nvDeviceEndpointOut);
		memset(nvdev->remainingData, 0, USB_MINIMUM_READ_LENGTH);
		nvdev->remainingSize = 0; 
	}

	nvdev->nvSequence = 0;
	nvdev->nvRecSequence = 0;
	*pnvdev = nvdev;

	return ret;
}

int nvusb_send(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int ret;
	int written;
	ret = libusb_bulk_transfer(nvdev->nvDeviceID, nvdev->nvDeviceEndpointOut, buffer, size, &written, 0);
	if(ret)
		printf("Error sending data (%d): %s\n", ret, libusb_error_name(ret));
	// Reset buffered data if we send data
	nvdev->remainingSize = 0;

	return ret;
}

static int32_t nvusb_receive_internal(nvDevice_t nvdev, uint8_t *buffer, int32_t size, int32_t *read){

	int ret;
	ret = libusb_bulk_transfer(nvdev->nvDeviceID, nvdev->nvDeviceEndpointIn, buffer, size, read, 0);

	return ret;
}

int nvusb_receive_unbuffered(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int read;
	int ret = nvusb_receive_internal(nvdev, buffer, size, &read);
	if(ret)
		printf("Error receiving data (%d): %s\n", ret, libusb_error_name(ret));
	return ret;
}

int nvusb_receive(nvDevice_t nvdev, uint8_t *buffer, int32_t size){
	int32_t ret, read, remaining=size;
	uint8_t *currBuf = buffer;
	uint8_t localBuf[USB_MINIMUM_READ_LENGTH];
	if(size <= nvdev->remainingSize)
	{
		memcpy(currBuf, &nvdev->remainingData, size);
		memmove(nvdev->remainingData, &nvdev->remainingData[size], nvdev->remainingSize);
		nvdev->remainingSize -= size;
		return size;
	}
	if(nvdev->remainingSize > 0)
	{
		memcpy(currBuf, nvdev->remainingData, nvdev->remainingSize);
		remaining -= nvdev->remainingSize;
		currBuf += nvdev->remainingSize;
		nvdev->remainingSize = 0;
	}
	ret = nvusb_receive_internal(nvdev, localBuf, MAX(USB_MINIMUM_READ_LENGTH, remaining), &read);
	memcpy(currBuf, localBuf, MIN(remaining, read));
	if(remaining < read)
	{
		memcpy(nvdev->remainingData, &localBuf[remaining], read-remaining);
		nvdev->remainingSize = read-remaining;
	}
	// Not quite correct necessarily.
	// We could theoretically have gotten less bytes than we asked for.
	return size; 
}

void nvusb_device_close(nvDevice_t *nvdev){

	libusb_release_interface((*nvdev)->nvDeviceID, 0);
	libusb_close((*nvdev)->nvDeviceID);
	libusb_exit(NULL);
	free(*nvdev);
	nvdev = NULL;
	return;

}


void nvusb_versions(){

	printf("USB library version is %s and is ABI version %s\n", USB_VERSION, USB_ABI);

}
