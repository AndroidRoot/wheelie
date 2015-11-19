/* libnvfblob.c - Library for parsing nvflash style blobs.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "libnvfblob.h"

nvfblob_file_t nvfblob_open(const char *filename)
{
    return (nvfblob_file_t)fopen(filename, "rb");
}

int nvfblob_close(nvfblob_file_t handle)
{
    return fclose((FILE *)handle);
}

int nvfblob_alloc_read(nvfblob_file_t handle, uint32_t type,
                       unsigned char **buf, size_t *length)
{
    FILE *fp = (FILE *)handle;
    nvfblob_header_t header = {0};

    /* Initialise *buf to the NULL pointer so we don't end up
     * attempting to free an uninitialised pointer due to a
     * failure condition. */
    *buf = NULL;

    /* Always start at the beginning of the file. */
    fseek(fp, 0, SEEK_SET);

    do {
        /* Read the blob header from the file. */
        if(fread(&header, sizeof(header), 1, fp) != 1) {
            goto fail;
        }

        /* Check if we've found the requested type. */
        if(header.type == type) {
            *length = header.length;

            /* Allocate memory for the data. */
            if(!(*buf = (unsigned char*)malloc(*length))) {
                goto fail;
            }

            /* Read the data from the blob file. */
            if(fread(*buf, sizeof(unsigned char), *length, fp) != *length) {
                goto fail;
            }

            return 1;
        } else {
            /* Advance the file pointer to the next blob header. */
            fseek(fp, header.length, SEEK_CUR);
        }
    } while(!feof(fp));

    /* End of the file and we haven't found the requested type. */
    errno = EINVAL;

fail:
    if(*buf) free(*buf);
    *buf = NULL;
    *length = 0;
    return 0;
}

void nvfblob_alloc_free(unsigned char **buf)
{
    free(*buf);
    *buf = NULL;
}

#ifdef DEBUG_STANDALONE
/* Quick implementation for testing purposes.
 *
 * $ gcc -I../include/ -DDEBUG_STANDALONE -o nvfblob libnvfblob.c
 */
int main(int argc, char **argv)
{
    unsigned char *buf;
    size_t len;

    FILE *out;
    nvfblob_file_t blob;

    if(argc != 4) {
        fprintf(stderr, "Usage: %s <blob file> <type> <out file>\n", argv[0]);
        return 1;
    }

    if(!(blob = nvfblob_open(argv[1]))) {
        perror("failed to open blob file");
        return 1;
    }

    if(!nvfblob_alloc_read(blob, atoi(argv[2]), &buf, &len)) {
        perror("failed to read blob/find requested type");
        return 1;
    }

    if(!(out = fopen(argv[3], "w"))) {
        perror("failed to open output file");
        return 1;
    }

    if(fwrite(buf, sizeof(unsigned char), len, out) != len) {
        perror("failed to write data to output file");
        return 1;
    }

    nvfblob_alloc_free(&buf);
    nvfblob_close(blob);
}
#endif
