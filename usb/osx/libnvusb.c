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
#include <CoreFoundation/CoreFoundation.h>

#include "config.h"
#include "shared.h"
#include "libnvusb.h"

static int32_t getEndpoints(IOUSBInterfaceInterface **interface, uint8_t *ep_in, uint8_t *ep_out){
    kern_return_t           kr;
    UInt8                       interfaceNumEndpoints;   
    int pipeRef;
    
    kr = (*interface)->GetNumEndpoints(interface,
                                       &interfaceNumEndpoints);
    for (pipeRef = 1; pipeRef <= interfaceNumEndpoints; pipeRef++)
    {
        IOReturn        kr2;
        UInt8           direction;
        UInt8           number;
        UInt8           transferType;
        UInt16          maxPacketSize;
        UInt8           interval;
        kr2 = (*interface)->GetPipeProperties(interface,
                                              pipeRef, &direction,
                                              &number, &transferType,
                                              &maxPacketSize, &interval);
        if(transferType == kUSBBulk)
        {
            if(direction == kUSBIn)
                *ep_in = pipeRef;
            if(direction == kUSBOut)
                *ep_out = pipeRef;
        }
    }
    DEBUGPRINT("Ep_In = %d, Ep_Out = %d\n", *ep_in, *ep_out);
    return 0;
}

IOUSBInterfaceInterface **FindInterface(IOUSBDeviceInterface **device)
{
  IOUSBFindInterfaceRequest   request;
  io_iterator_t               iterator;
  io_service_t                usbInterface;
  HRESULT                     result;
  IOReturn                    kr;
  IOCFPlugInInterface         **plugInInterface = NULL;
  IOUSBInterfaceInterface     **interface = NULL;
  SInt32              score;

  request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
 
  //Get an iterator for the interfaces on the device
  kr = (*device)->CreateInterfaceIterator(device,
                                        &request, &iterator);
   if (usbInterface = IOIteratorNext(iterator))
   {
        //Create an intermediate plug-in
        kr = IOCreatePlugInInterfaceForService(usbInterface,
                            kIOUSBInterfaceUserClientTypeID,
                            kIOCFPlugInInterfaceID,
                            &plugInInterface, &score);
        //Release the usbInterface object after getting the plug-in
        kr = IOObjectRelease(usbInterface);
        result = (*plugInInterface)->QueryInterface(plugInInterface,
                    CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                    (LPVOID *) &interface);
        //No longer need the intermediate plug-in
        (*plugInInterface)->Release(plugInInterface);
   }
   return interface;
}

int nvusb_detect_device(int vendid, int32_t prodid, nvDevice_t* pnvdev){
	// declarations
    int ret=0;
    CFMutableDictionaryRef  matchingDict;
    CFNumberRef             numberRef;
    kern_return_t           kr;
    io_iterator_t            gAddedIter;
    io_service_t        usbDevice;
    SInt32              score;
    HRESULT             res;
    
    *pnvdev = NULL; 
    nvDevice_t nvdev;
    nvdev = (nvDevice_t) malloc(sizeof(struct nvDevice));
    memset(nvdev, 0, sizeof(struct nvDevice));
    
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if(matchingDict == NULL)
    {
        ret = -1;
        goto error;
    }
    
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendid);
    CFDictionarySetValue(matchingDict, 
                         CFSTR(kUSBVendorID), 
                         numberRef);
    CFRelease(numberRef);
    
    // Create a CFNumber for the idProduct and set the value in the dictionary
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &prodid);
    CFDictionarySetValue(matchingDict, 
                         CFSTR(kUSBProductID), 
                         numberRef);
    CFRelease(numberRef);
    numberRef = NULL;
    
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &gAddedIter);
    // We only support one apx mode device connected at a time
    if((usbDevice = IOIteratorNext(gAddedIter))) {
        IOCFPlugInInterface **plugInInterface = NULL;
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
                                               &plugInInterface, &score);
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                 (LPVOID*) &nvdev->deviceinterface);
        (*plugInInterface)->Release(plugInInterface);
	nvdev->interface = FindInterface(nvdev->deviceinterface);
        kr = IOObjectRelease(usbDevice);
        kr = (*nvdev->deviceinterface)->USBDeviceOpen(nvdev->deviceinterface);

        kr = (*nvdev->interface)->USBInterfaceOpen(nvdev->interface);
        getEndpoints(nvdev->interface, &nvdev->nvDeviceEndpointIn, &nvdev->nvDeviceEndpointOut);
	// We only use synchronous IO, so close device
        kr = (*nvdev->deviceinterface)->USBDeviceClose(nvdev->deviceinterface);
        kr = (*nvdev->deviceinterface)->Release(nvdev->deviceinterface);

    }
    IOObjectRelease(gAddedIter);
    if(nvdev->deviceinterface == NULL || nvdev->interface == NULL)
    {
        ret = -1;
        goto error;
    }
    memset(nvdev->remainingData, 0, USB_MINIMUM_READ_LENGTH);
    nvdev->remainingSize = 0; 

    nvdev->nvSequence = 0;
    nvdev->nvRecSequence = 0;

    *pnvdev = nvdev;

    goto exit;
    
error:
    free(nvdev);
    *pnvdev = NULL;
    
exit:
	return ret;
}

int nvusb_send(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int ret=0;
	int written;
        IOUSBInterfaceInterface **interface = (IOUSBInterfaceInterface**)nvdev->interface;
	DEBUGPRINT("Writing %d bytes to endpoint %d\n", size, nvdev->nvDeviceEndpointOut);
        ret = (*interface)->WritePipe(interface, nvdev->nvDeviceEndpointOut, buffer, size);
        DEBUGPRINT("Result %d\n", ret);
	// Reset buffered data if we send data
	nvdev->remainingSize = 0;

	return ret;
}

static int32_t nvusb_receive_internal(nvDevice_t nvdev, uint8_t *buffer, int32_t size, int32_t *read){
	int ret=0;
    IOUSBInterfaceInterface **interface = (IOUSBInterfaceInterface**)nvdev->interface;
    *read = size;
    (*interface)->ReadPipe(interface, nvdev->nvDeviceEndpointIn, buffer, (UInt32*)read);
	return ret;
}

int nvusb_receive_unbuffered(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int read;
	return nvusb_receive_internal(nvdev, buffer, size, &read);
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
    
	return;

}


void nvusb_versions(){

	printf("USB library version is %s and is ABI version %s\n", USB_VERSION, USB_ABI);

}
