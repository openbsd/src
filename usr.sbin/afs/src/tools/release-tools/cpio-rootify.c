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
#include <err.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

/*
 * This only works for "new" cpio archives, those with ASCII header
 */

/* from netbsd pax */

struct cpio_header {
    char magic[6];		/* magic cookie */
    char dev[6];		/* device number */
    char ino[6];		/* inode number */
    char mode[6];		/* file type/access */
    char uid[6];		/* owners uid */
    char gid[6];		/* owners gid */
    char nlink[6];		/* # of links at archive creation */
    char rdev[6];		/* block/char major/minor # */
    char mtime[11];		/* modification time */
    char namesize[6];		/* length of pathname */
    char filesize[11];		/* length of file in bytes */
};

#define	MAGIC		070707		/* transportable archive id */


int
main(int argc, char **argv)
{
    char *buf;
    int  rec = 0;
    uint64_t skipbytes;
    struct cpio_header *p;
    uint64_t val, filesize, namesize, uid, gid;
    int verbose = 0;

    buf = malloc(sizeof(*p));
    if (buf == NULL)
	err(1, "malloc");
    
    skipbytes = 0;

    while (read(0, buf, sizeof(*p)) == sizeof(*p)) {
	p = (void *)buf;

	rec++;

	val = estrntoll(p->magic, sizeof(p->magic), 8);
	if (val != MAGIC)
	    errx(1, "rec #%d containted wrong magic", rec);

	namesize = estrntoll(p->namesize, sizeof(p->namesize), 8);
	filesize = estrntoll(p->filesize, sizeof(p->filesize), 8);
	uid = estrntoll(p->uid, sizeof(p->uid), 8);
	gid = estrntoll(p->uid, sizeof(p->gid), 8);

	if (verbose) {
	    fprintf(stderr, "raw-uid: %.*s\n", (int)sizeof(p->uid), p->uid);
	    fprintf(stderr, "raw-gid: %.*s\n", (int)sizeof(p->gid), p->gid);

	    fprintf(stderr,
		    "namesize %d\nfilesize %d\n"
		    "uid: %d\ngid %d\n",
		    (int)namesize, (int) filesize,
		    (int)uid, (int)gid);
	}

	{ 
	    char dbuf[7];
	    
	    snprintf(dbuf, sizeof(dbuf), "%06o", 0);
	    memcpy(p->uid, dbuf, 6);
	    snprintf(dbuf, sizeof(dbuf), "%06o", 0);
	    memcpy(p->gid, dbuf, 6);
	}

	if (write(1, buf, sizeof(*p)) != sizeof(*p))
	    errx(1, "write");

	skipbytes = filesize;

	if (namesize == 11) {
	    char *namebuf = malloc(namesize);
	    if (namebuf == NULL)
		errx(1, "malloc");
	    if (read(0, namebuf, (int)namesize) != (int)namesize)
		errx(1, "read");
	    if (write(1, namebuf, (int)namesize) != (int)namesize)
		err(1, "write");
	    
	    if (memcmp("TRAILER!!!", namebuf, 11) == 0)
		break;
	    /* XXXX 
	       this break make us not copy the data after the TRAILER!!! */
	} else
	    skipbytes += namesize;

	{
	    char buf[1024 * 8];
	    int i;

	    do {
		i = sizeof(buf);
		if (i > skipbytes)
		    i = skipbytes;

		if (read(0, buf, i) != i)
		    err(1, "read");
		if (write(1, buf, i) != i)
		    err(1, "write");
		skipbytes -= i;
	    } while (skipbytes);
	}
    }

    return 0;
}
