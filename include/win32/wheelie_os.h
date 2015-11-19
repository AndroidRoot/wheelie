/* libnvfblob.c - Library for parsing nvflash style blobs.
 *
 * Copyright (C) 2015 androidroot.mobi
 *
 * This file is part of wheelie.
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


#ifndef _WHEELIE_OS_H_
#define _WHEELIE_OS_H_

#include <BaseTsd.h>

typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef INT8 int8_t;
typedef INT16 int16_t;
typedef INT32 int32_t;
typedef INT64 int64_t;

//typedef SIZE_T size_t;
#define _SSIZE_T_DEFINED 1
typedef SSIZE_T ssize_t;

#define usleep(x) Sleep((x)/1000)
#endif
