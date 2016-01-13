/*	$OpenBSD: file_media.c,v 1.19 2016/01/13 00:12:49 krw Exp $	*/

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
typedef struct file_media *FILE_MEDIA;

struct file_media {
    struct media	m;
    int			fd;
    int			regular_file;
};

struct file_media_globals {
    long		exists;
    long		kind;
};

/*
 * Global Constants
 */
int potential_block_sizes[] = {
    1, 512, 1024, 2048,
    0
};


/*
 * Global Variables
 */
static long file_inited = 0;
static struct file_media_globals file_info;

/*
 * Forward declarations
 */
int compute_block_size(int fd);
void file_init(void);
FILE_MEDIA new_file_media(void);
long read_file_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_file_media(MEDIA m, long long offset, unsigned long count, void *address);
long close_file_media(MEDIA m);
long os_reload_file_media(MEDIA m);

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

    file_info.kind = allocate_media_kind();
}


FILE_MEDIA
new_file_media(void)
{
    return (FILE_MEDIA) new_media(sizeof(struct file_media));
}


int
compute_block_size(int fd)
{
    int size;
    int max_size;
    off_t x;
    long t;
    int i;
    char *buffer;

    max_size = 0;
    for (i = 0; ; i++) {
    	size = potential_block_sizes[i];
    	if (size == 0) {
	    break;
    	}
    	if (max_size < size) {
	    max_size = size;
    	}
    }

    buffer = malloc(max_size);
    if (buffer != 0) {
	for (i = 0; ; i++) {
	    size = potential_block_sizes[i];
	    if (size == 0) {
		break;
	    }
	    if ((x = lseek(fd, 0, SEEK_SET)) < 0) {
		warn("Can't seek on file");
		break;
	    }
	    if ((t = read(fd, buffer, size)) == size) {
		free(buffer);
		return size;
	    }
	}
    }
    free(buffer);
    return 0;
}


MEDIA
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
	a = new_file_media();
	if (a != 0) {
	    a->m.kind = file_info.kind;
	    a->m.grain = compute_block_size(fd);
	    off = lseek(fd, 0, SEEK_END);	/* seek to end of media */
	    //printf("file size = %Ld\n", off);
	    a->m.size_in_bytes = (long long) off;
	    a->m.do_read = read_file_media;
	    a->m.do_write = write_file_media;
	    a->m.do_close = close_file_media;
	    a->m.do_os_reload = os_reload_file_media;
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
    return (MEDIA) a;
}


long
read_file_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    FILE_MEDIA a;
    long rtn_value;
    off_t off;
    int t;

    a = (FILE_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
	fprintf(stderr,"no media\n");
    } else if (a->m.kind != file_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
	fprintf(stderr,"wrong kind\n");
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
	fprintf(stderr,"bad size\n");
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
	fprintf(stderr,"bad offset\n");
    } else if (offset + count > a->m.size_in_bytes && a->m.size_in_bytes != (long long) 0) {
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
write_file_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    FILE_MEDIA a;
    long rtn_value;
    off_t off;
    int t;

    a = (FILE_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != file_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (count > LLONG_MAX - offset) {
	/* check for offset (and offset+count) too large */
    } else {
	/* do the write  */
	off = offset;
	if ((off = lseek(a->fd, off, SEEK_SET)) >= 0) {
	    if ((t = write(a->fd, address, count)) == count) {
		if (off + count > a->m.size_in_bytes) {
			a->m.size_in_bytes = off + count;
		}
		rtn_value = 1;
	    }
	}
    }
    return rtn_value;
}


long
close_file_media(MEDIA m)
{
    FILE_MEDIA a;

    a = (FILE_MEDIA) m;
    if (a == 0) {
	return 0;
    } else if (a->m.kind != file_info.kind) {
	/* XXX need to error here - this is an internal problem */
	return 0;
    }

    close(a->fd);
    return 1;
}


long
os_reload_file_media(MEDIA m)
{
    FILE_MEDIA a;
    long rtn_value;

    a = (FILE_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != file_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (a->regular_file) {
	/* okay - nothing to do */
	rtn_value = 1;
    } else {
	rtn_value = 1;
    }
    return rtn_value;
}
