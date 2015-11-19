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
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "config.h"
#include "shared.h"


// Include Windows headers
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

// Include WinUSB headers
#include <winusb.h>
#include <Usb100.h>
#include <Setupapi.h>
#include "libnvusb.h"
//{36fc9e60-c465-11cf-8056-444553540000}
static const GUID GUID_CLASS_APX = {0x36fc9e60L, 0xc465, 0x11cf, {0x80, 0x56, 0x36, 0xE6, 0xD7, 0x20, 0x46, 0x00}};
static const GUID GUID_DEVINTERFACE_APX = {0xEAD8C4F6L, 0x6102, 0x45c7, {0xAA, 0x66, 0x36, 0xE6, 0xD7, 0x20, 0x46, 0x00}};

static int32_t getEndpoints(WINUSB_INTERFACE_HANDLE hDeviceHandle, uint8_t *ep_in, uint8_t *ep_out){
	int32_t i;
	int32_t ret=-2;
    USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
    WINUSB_PIPE_INFORMATION  Pipe;

	memset(&InterfaceDescriptor, 0, sizeof(USB_INTERFACE_DESCRIPTOR));
	memset(&Pipe, 0, sizeof(WINUSB_PIPE_INFORMATION));
    
    ret = WinUsb_QueryInterfaceSettings(hDeviceHandle, 0, &InterfaceDescriptor);

    if (ret)
    {
        for (i = 0; i < InterfaceDescriptor.bNumEndpoints; i++)
        {
            ret = WinUsb_QueryPipe(hDeviceHandle, 0, i, &Pipe);

            if (ret)
            {
                if (Pipe.PipeType == UsbdPipeTypeBulk)
                {
                    if (USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId))
                    {
                        DEBUGPRINT("Endpoint index: %d Pipe type: Bulk Pipe ID: %c.\n", i, Pipe.PipeType, Pipe.PipeId);
                        *ep_in = Pipe.PipeId;
                    }
                    if (USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId))
                    {
                        DEBUGPRINT("Endpoint index: %d Pipe type: Bulk Pipe ID: %c.\n", i, Pipe.PipeType, Pipe.PipeId);
                        *ep_out = Pipe.PipeId;
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
	return ret;
}

static char* getDevicePath(const GUID* guid)
{
	char * ret = NULL;
	int i;
	SP_DEVICE_INTERFACE_DATA currentInterface;
	HDEVINFO devices;
	devices = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if(devices == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	currentInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	for(i = 0; SetupDiEnumDeviceInterfaces(devices, NULL, &GUID_DEVINTERFACE_APX, i, &currentInterface); i++) {
		DWORD requiredSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA details;
		SetupDiGetDeviceInterfaceDetail(devices, &currentInterface, NULL, 0, &requiredSize, NULL);
		details = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredSize);
		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(devices, &currentInterface, details, requiredSize, NULL, NULL)) {
			free(details);
			break;
		} else {
			char* result = (char *) malloc(requiredSize);
			char* pathEnd = NULL;
			memcpy((void*) result, details->DevicePath, requiredSize);
			ret=0;
			free(details);
			ret = result;
			break;
			
		}
	}
	SetupDiDestroyDeviceInfoList(devices);
	return ret;
}

int nvusb_detect_device(int vendid, int32_t prodid, nvDevice_t* pnvdev){
	// declarations
	int ret=-1,i;
	char * nvAPXPath;

	nvDevice_t nvdev = (nvDevice_t)malloc(sizeof(struct nvDevice));
	memset(nvdev, 0, sizeof(struct nvDevice));

	nvAPXPath = getDevicePath(&GUID_DEVINTERFACE_APX);
	if(nvAPXPath == NULL)
	{
		DEBUGPRINT("Error getting devicePath: %d\n", ret);
		return -1;
	}

	nvdev->nvAPX = CreateFile(nvAPXPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if(nvdev->nvAPX == INVALID_HANDLE_VALUE)
	{
		DEBUGPRINT("Error %d.", GetLastError());
		free(nvAPXPath);
		return -2;
	}
	free(nvAPXPath);
	ret = WinUsb_Initialize(nvdev->nvAPX, &nvdev->nvWinUSBAPX);
	if(!ret)
	{
		DEBUGPRINT("Error %d.", GetLastError());
		CloseHandle(nvdev->nvAPX);
		return -3;
	}

	getEndpoints(nvdev->nvWinUSBAPX, &nvdev->nvDeviceEndpointIn, &nvdev->nvDeviceEndpointOut);

	*pnvdev = nvdev;
	return ret;
}

int nvusb_send(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int ret=-1;
	ULONG written=0;

	ret = WinUsb_WritePipe(nvdev->nvWinUSBAPX, nvdev->nvDeviceEndpointOut, buffer, size, &written, NULL);
	if(!ret)
	{
		DEBUGPRINT("Error writing data %d\n", GetLastError());
		return -1;
	}
	// Reset buffered data if we send data
	nvdev->remainingSize = 0;

	return 0;
}

static int32_t nvusb_receive_internal(nvDevice_t nvdev, uint8_t *buffer, int32_t size, int32_t *read){
	int ret=-1;
	ULONG readdata=0;
	ret = WinUsb_ReadPipe(nvdev->nvWinUSBAPX, nvdev->nvDeviceEndpointIn, buffer, size, &readdata, NULL);
	if(!ret)
	{
		int err = GetLastError();
		DEBUGPRINT("Error reading data %d\n", err);
		switch(err)
		{
		case ERROR_INVALID_HANDLE:
			DEBUGPRINT("ERROR_INVALID_HANDLE\n");
			break;
		case ERROR_IO_PENDING:
			DEBUGPRINT("ERROR_IO_PENDING\n");
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
			DEBUGPRINT("ERROR_NOT_ENOUGH_MEMORY\n");
			break;
		case ERROR_SEM_TIMEOUT:
			DEBUGPRINT("ERROR_SEM_TIMEOUT\n");
			break;
		default:
			DEBUGPRINT("Uknown error!\n");
			break;
		}
		return -1;
	}
	*read = (int32_t)readdata;
	return 0;
}

int nvusb_receive_unbuffered(nvDevice_t nvdev, uint8_t *buffer, int32_t size){

	int read;
	// Reset buffered data if we receive unbuffered.s
	nvdev->remainingSize = 0;
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
	WinUsb_Free((*nvdev)->nvWinUSBAPX);
	CloseHandle((*nvdev)->nvAPX);
	free(*nvdev);
	nvdev = NULL;
	return;

}


void nvusb_versions(){

	printf("USB library version is %s and is ABI version %s\n", USB_VERSION, USB_ABI);

}
