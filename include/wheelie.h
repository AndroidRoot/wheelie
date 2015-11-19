/* wheelie.h - Preflight for nvflash
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

#include <getopt.h>

int filetest(const char* filename, size_t *filesize){
	struct stat info;
	int ret = -1;
	uint32_t fsize;

	ret = stat(filename, &info);
	fsize = info.st_size;
	*filesize = fsize;
	if(ret == 0){
		return 1;
	}else{
		return 0;
	}
}
