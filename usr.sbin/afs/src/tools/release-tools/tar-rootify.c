/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <atypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <err.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

/* from gnu tar manual */

struct posix_header
{                               /* byte offset */
  char name[100];               /*   0 */
  char mode[8];                 /* 100 */
  char uid[8];                  /* 108 */
  char gid[8];                  /* 116 */
  char size[12];                /* 124 */
  char mtime[12];               /* 136 */
  char chksum[8];               /* 148 */
  char typeflag;                /* 156 */
  char linkname[100];           /* 157 */
  char magic[6];                /* 257 */
  char version[2];              /* 263 */
  char uname[32];               /* 265 */
  char gname[32];               /* 297 */
  char devmajor[8];             /* 329 */
  char devminor[8];             /* 337 */
  char prefix[155];             /* 345 */
                                /* 500 */
};

#define TMAGIC   "ustar"        /* ustar and a null */
#define OLDGNU_MAGIC "ustar  "  /* 7 chars and a null */
#define TMAGLEN  6
#define TVERSION "00"           /* 00 and no null */
#define TVERSLEN 2

int
main(int argc, char **argv)
{
    char *buf;
    int bufsz = 512;
    int i, skip = 0, rec = 0;
    struct posix_header *p;
    uint64_t size;
    unsigned long cksum, hcksum;
    int verbose = 0;

    buf = malloc(bufsz);
    if (buf == NULL)
	err(1, "malloc");
    
    while (read(0, buf, bufsz) == bufsz) {
	rec++;
	p = (void *)buf;

	if (skip || p->name[0] == '\0') {
	    if (write(1, buf, bufsz) != bufsz)
		err(1, "write");
	    skip--;
	    continue;
	}

	if (strcmp(p->magic, TMAGIC) != 0
	    && strcmp(p->magic, OLDGNU_MAGIC) != 0)
	    errx(1, "bad magic in #%d  '%.*s'", rec, 
		 (int)sizeof(p->magic), p->magic);

	cksum = 256;
	for (i = 0; i < sizeof(*p); i++)
	    if (i < 148 || 155 < i)
		cksum += (u_long)(buf[i] & 0xff);

	if (verbose) {
	    fprintf(stderr, "rec #%d\n", rec);
	    fprintf(stderr, "name = %.*s\n", (int)sizeof(p->name), p->name);
	    fprintf(stderr, "uid = %.*s\n", (int)sizeof(p->uid), p->uid);
	    fprintf(stderr, "gid = %.*s\n", (int)sizeof(p->gid), p->gid);
	    fprintf(stderr, "uname = %.*s\n", (int)sizeof(p->uname), p->uname);
	    fprintf(stderr, "gname = %.*s\n", (int)sizeof(p->gname), p->gname);
	    fprintf(stderr, "type =     %c\n", p->typeflag);
	    fprintf(stderr, "size = %.*s\n", (int)sizeof(p->size), p->size);
	    size = estrntoll(p->size, 12, 8);
	    fprintf(stderr, "size = %llo\n", size);
	}

	hcksum = estrntoll(p->chksum, 8, 8);
	if (hcksum != cksum)
	    errx(1, "invalid cksum %d != %d", (int)hcksum, (int)cksum);
	snprintf(p->uid, (int)sizeof(p->uid), "%6o ", 0);
	snprintf(p->gid, (int)sizeof(p->gid), "%6o ", 0);
	snprintf(p->uname, (int)sizeof(p->uname), "root");
	snprintf(p->gname, (int)sizeof(p->gname), "wheel");

	cksum = 256;
	for (i = 0; i < sizeof(*p); i++)
	    if (i < 148 || 155 < i)
		cksum += (u_long)(buf[i] & 0xff);

	snprintf(p->chksum, (int)sizeof(p->chksum), " %6o ", (unsigned)cksum);

	if (write(1, buf, bufsz) != bufsz)
	    errx(1, "write");

	if (size)
	    skip = (size + bufsz - 1) / bufsz;
    }
    return 0;
}
