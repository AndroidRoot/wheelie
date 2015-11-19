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

#ifndef _LIBNVUSB_H_
#define _LIBNVUSB_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define USB_MINIMUM_READ_LENGTH 64
#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

struct nvDevice;
typedef struct nvDevice* nvDevice_t;

struct nvDevice{

	int nvDeviceDetected;
	int nvDeviceRead;
	// Defined as void pointers to not infect libnvusb.h with WinUSB stuff.
	void* nvAPX;
	void* nvWinUSBAPX;
	uint8_t nvDeviceEndpointIn;
	uint8_t nvDeviceEndpointOut;
	uint32_t nvSequence;
	uint32_t nvRecSequence;

    // Buffer to store data 
    // Bulk reads have to be minimum 64 bytes, so 
    uint8_t remainingData[USB_MINIMUM_READ_LENGTH];
    int32_t remainingSize;
};

int nvusb_detect_device(int prodid, int32_t vendid, nvDevice_t* pnvdev);

int nvusb_send(nvDevice_t nvdev, uint8_t *buffer, int32_t size);

int nvusb_receive(nvDevice_t nvdev, uint8_t *buffer, int32_t size);

int nvusb_receive_unbuffered(nvDevice_t nvdev, uint8_t *buffer, int32_t size);

void nvusb_device_close(nvDevice_t* nvdev);

void nvusb_versions();

#endif /* _LIBNVUSB_H_ */
