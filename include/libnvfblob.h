/* libnvfblob.c - Library for parsing nvflash style blobs.
 *
 * Copyright (C) 2015 androidroot.mobi
 *
 * This file is part of libnvfblob.
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

#ifndef LIBNVFBLOB_H
#define LIBNVFBLOB_H
#include <stdint.h>

#define NVFBLOB_HASH_SIZE 16

typedef struct {
    enum {
        NVFBLOB_TYPE_VERSION = 0x01,
        NVFBLOB_TYPE_RCMVER,
        NVFBLOB_TYPE_RCMDLEXEC,
        NVFBLOB_TYPE_BLHASH,
        NVFBLOB_TYPE_EXT_WHEELIE_BCTC = 0x7F000000,
        NVFBLOB_TYPE_EXT_WHEELIE_BCTR,
        NVFBLOB_TYPE_EXT_WHEELIE_BL,
        NVFBLOB_TYPE_EXT_WHEELIE_ODMDATA,
        NVFBLOB_TYPE_EXT_WHEELIE_CPU_ID,
        NVFBLOB_TYPE_FORCE32 = 0x7FFFFFFF
    } type;
    uint32_t length;
    uint32_t reserved1;
    uint32_t reserved2;
    unsigned char hash[NVFBLOB_HASH_SIZE];
} nvfblob_header_t;

typedef void * nvfblob_file_t;

nvfblob_file_t nvfblob_open(const char *);
int nvfblob_alloc_read(nvfblob_file_t, uint32_t, unsigned char **, size_t *);
void nvfblob_alloc_free(unsigned char **);
int nvfblob_close(nvfblob_file_t);
#endif
