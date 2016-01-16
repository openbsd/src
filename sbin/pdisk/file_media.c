/*	$OpenBSD: file_media.c,v 1.26 2016/01/16 22:28:14 krw Exp $	*/

/*
 * file_media.c -
 *
 * Written by Eryk Vershen
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/dkio.h>
#include <sys/disklabel.h>
#include <err.h>

// for printf()
#include <stdio.h>
// for malloc() & free()
#include <stdlib.h>
// for lseek(), read(), write(), close()
#include <unistd.h>
// for open()
#include <fcntl.h>
// for LONG_MAX
#include <limits.h>
// for errno
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <util.h>

#include "file_media.h"


/*
 * Types
 */


/*
 * Global Constants
 */


/*
 * Global Variables
 */
static long file_inited = 0;

/*
 * Forward declarations
 */
void compute_block_size(int fd, char *name);
void file_init(void);

/*
 * Routines
 */
void
file_init(void)
{
    if (file_inited != 0) {
	return;
    }
    file_inited = 1;
}


void
compute_block_size(int fd, char *name)
{
	struct disklabel dl;
	struct stat st;

	if (fstat(fd, &st) == -1)
		err(1, "can't fstat %s", name);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", name);
	if (ioctl(fd, DIOCGPDINFO, &dl) == -1)
		err(1, "can't get disklabel for %s", name);

	if (dl.d_secsize != DEV_BSIZE)
		err(1, "%u-byte sector size not supported", dl.d_secsize);
}


FILE_MEDIA
open_file_as_media(char *file, int oflag)
{
    FILE_MEDIA	a;
    int			fd;
    off_t off;
    struct stat info;

    if (file_inited == 0) {
	    file_init();
    }

    a = 0;
    fd = opendev(file, oflag, OPENDEV_PART, NULL);
    if (fd >= 0) {
	a = malloc(sizeof(struct file_media));
	if (a != 0) {
	    compute_block_size(fd, file);
	    off = lseek(fd, 0, SEEK_END);	/* seek to end of media */
	    //printf("file size = %Ld\n", off);
	    a->size_in_bytes = (long long) off;
	    a->fd = fd;
	    a->regular_file = 0;
	    if (fstat(fd, &info) < 0) {
		warn("can't stat file '%s'", file);
	    } else {
		a->regular_file = S_ISREG(info.st_mode);
	    }
	} else {
	    close(fd);
	}
    }
    return (FILE_MEDIA) a;
}


long
read_file_media(FILE_MEDIA a, long long offset, unsigned long count,
    void *address)
{
    long rtn_value;
    off_t off;
    int t;

    rtn_value = 0;
    if (a == 0) {
	/* no media */
	fprintf(stderr,"no media\n");
    } else if (count <= 0 || count % DEV_BSIZE != 0) {
	/* can't handle size */
	fprintf(stderr,"bad size\n");
    } else if (offset < 0 || offset % DEV_BSIZE != 0) {
	/* can't handle offset */
	fprintf(stderr,"bad offset\n");
    } else if (offset + count > a->size_in_bytes && a->size_in_bytes != (long long) 0) {
	/* check for offset (and offset+count) too large */
	fprintf(stderr,"offset+count too large\n");
    } else if (count > LLONG_MAX - offset) {
	/* check for offset (and offset+count) too large */
	fprintf(stderr,"offset+count too large 2\n");
    } else {
	/* do the read */
	off = offset;
	if ((off = lseek(a->fd, off, SEEK_SET)) >= 0) {
	    if ((t = read(a->fd, address, count)) == count) {
		rtn_value = 1;
	    } else {
		fprintf(stderr,"read failed\n");
	    }
	} else {
	    fprintf(stderr,"lseek failed\n");
	}
    }
    return rtn_value;
}


long
write_file_media(FILE_MEDIA a, long long offset, unsigned long count, void *address)
{
    long rtn_value;
    off_t off;
    int t;

    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (count <= 0 || count % DEV_BSIZE != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % DEV_BSIZE != 0) {
	/* can't handle offset */
    } else if (count > LLONG_MAX - offset) {
	/* check for offset (and offset+count) too large */
    } else {
	/* do the write  */
	off = offset;
	if ((off = lseek(a->fd, off, SEEK_SET)) >= 0) {
	    if ((t = write(a->fd, address, count)) == count) {
		if (off + count > a->size_in_bytes) {
			a->size_in_bytes = off + count;
		}
		rtn_value = 1;
	    }
	}
    }
    return rtn_value;
}


long
close_file_media(FILE_MEDIA a)
{
    if (a == 0) {
	return 0;
    }

    close(a->fd);
    return 1;
}


long
os_reload_file_media(FILE_MEDIA a)
{
    long rtn_value;

    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->regular_file) {
	/* okay - nothing to do */
	rtn_value = 1;
    } else {
	rtn_value = 1;
    }
    return rtn_value;
}
