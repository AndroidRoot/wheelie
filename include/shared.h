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

#ifndef _WHEELIE_SHARED_H_
#define _WHEELIE_SHARED_H_

#include "wheelie_os.h"

#ifdef DEBUGOUTPUT
#define DEBUGPRINT(...) { printf(__VA_ARGS__); }
#else
#define DEBUGPRINT(...)
#endif

#endif /* _WHEELIE_SHARED_H_ */
