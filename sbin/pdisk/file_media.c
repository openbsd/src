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

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <sys/stat.h>
#else
#ifdef __unix__
#include <sys/ioctl.h>
#include <sys/stat.h>
#endif
#endif

#include "file_media.h"
#include "errors.h"


/*
 * Defines
 */
#ifdef __linux__
#define LOFF_MAX 9223372036854775807LL
extern __loff_t llseek (int __fd, __loff_t __offset, int __whence);
#elif defined(__OpenBSD__) || defined(__APPLE__)
#define loff_t off_t
#define llseek lseek
#define LOFF_MAX LLONG_MAX
#else
#define loff_t long
#define llseek lseek
#define LOFF_MAX LONG_MAX
#endif


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

typedef struct file_media_iterator *FILE_MEDIA_ITERATOR;

struct file_media_iterator {
    struct media_iterator   m;
    long		    style;
    long		    index;
};


/*
 * Global Constants
 */
int potential_block_sizes[] = {
    1, 512, 1024, 2048,
    0
};

enum {
    kSCSI_Disks = 0,
    kATA_Devices = 1,
    kSCSI_CDs = 2,
    kMaxStyle = 2
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
FILE_MEDIA_ITERATOR new_file_iterator(void);
void reset_file_iterator(MEDIA_ITERATOR m);
char *step_file_iterator(MEDIA_ITERATOR m);
void delete_file_iterator(MEDIA_ITERATOR m);


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
    loff_t x;
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
	    if ((x = llseek(fd, (loff_t)0, SEEK_SET)) < 0) {
		error(errno, "Can't seek on file");
		break;
	    }
	    if ((t = read(fd, buffer, size)) == size) {
		free(buffer);
		return size;
	    }
	}
    }
    return 0;
}


MEDIA
open_file_as_media(char *file, int oflag)
{
    FILE_MEDIA	a;
    int			fd;
    loff_t off;
#if defined(__linux__) || defined(__unix__)
    struct stat info;
#endif
	
    if (file_inited == 0) {
	    file_init();
    }

    a = 0;
    fd = open(file, oflag);
    if (fd >= 0) {
	a = new_file_media();
	if (a != 0) {
	    a->m.kind = file_info.kind;
	    a->m.grain = compute_block_size(fd);
	    off = llseek(fd, (loff_t)0, SEEK_END);	/* seek to end of media */
#if !defined(__linux__) && !defined(__unix__)
	    if (off <= 0) {
		off = 1; /* XXX not right? */
	    }
#endif
	    //printf("file size = %Ld\n", off);
	    a->m.size_in_bytes = (long long) off;
	    a->m.do_read = read_file_media;
	    a->m.do_write = write_file_media;
	    a->m.do_close = close_file_media;
	    a->m.do_os_reload = os_reload_file_media;
	    a->fd = fd;
	    a->regular_file = 0;
#if defined(__linux__) || defined(__unix__)
	    if (fstat(fd, &info) < 0) {
		error(errno, "can't stat file '%s'", file);
	    } else {
		a->regular_file = S_ISREG(info.st_mode);
	    }
#endif
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
    loff_t off;
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
    } else if (offset + count > (long long) LOFF_MAX) {
	/* check for offset (and offset+count) too large */
	fprintf(stderr,"offset+count too large 2\n");
    } else {
	/* do the read */
	off = offset;
	if ((off = llseek(a->fd, off, SEEK_SET)) >= 0) {
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
    loff_t off;
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
    } else if (offset + count > (long long) LOFF_MAX) {
	/* check for offset (and offset+count) too large */
    } else {
	/* do the write  */
	off = offset;
	if ((off = llseek(a->fd, off, SEEK_SET)) >= 0) {
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
#if defined(__linux__)
    int i;
    int saved_errno;
#endif
	
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
#ifdef __linux__
	sync();
	sleep(2);
	if ((i = ioctl(a->fd, BLKRRPART)) != 0) {
	    saved_errno = errno;
	} else {
	    // some kernel versions (1.2.x) seem to have trouble
	    // rereading the partition table, but if asked to do it
	    // twice, the second time works. - biro@yggdrasil.com */
	    sync();
	    sleep(2);
	    if ((i = ioctl(a->fd, BLKRRPART)) != 0) {
		saved_errno = errno;
	    }
	}

	// printf("Syncing disks.\n");
	sync();
	sleep(4);		/* for sync() */

	if (i < 0) {
	    error(saved_errno, "Re-read of partition table failed");
	    printf("Reboot your system to ensure the "
		    "partition table is updated.\n");
	}
#endif
	rtn_value = 1;
    }
    return rtn_value;
}


#if !defined(__linux__) && !defined(__unix__)
#pragma mark -
#endif


FILE_MEDIA_ITERATOR
new_file_iterator(void)
{
    return (FILE_MEDIA_ITERATOR) new_media_iterator(sizeof(struct file_media_iterator));
}


MEDIA_ITERATOR
create_file_iterator(void)
{
    FILE_MEDIA_ITERATOR a;

    if (file_inited == 0) {
	file_init();
    }

    a = new_file_iterator();
    if (a != 0) {
	a->m.kind = file_info.kind;
	a->m.state = kInit;
	a->m.do_reset = reset_file_iterator;
	a->m.do_step = step_file_iterator;
	a->m.do_delete = delete_file_iterator;
	a->style = 0;
	a->index = 0;
    }

    return (MEDIA_ITERATOR) a;
}


void
reset_file_iterator(MEDIA_ITERATOR m)
{
    FILE_MEDIA_ITERATOR a;

    a = (FILE_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != file_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (a->m.state != kInit) {
	a->m.state = kReset;
    }
}


char *
step_file_iterator(MEDIA_ITERATOR m)
{
    FILE_MEDIA_ITERATOR a;
    char *result;
    struct stat info;
    int	fd;
    int bump;
    int value;

    a = (FILE_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != file_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else {
	switch (a->m.state) {
	case kInit:
	    a->m.state = kReset;
	    /* fall through to reset */
	case kReset:
	    a->style = 0 /* first style */;
	    a->index = 0 /* first index */;
	    a->m.state = kIterating;
	    /* fall through to iterate */
	case kIterating:
	    while (1) {
		if (a->style > kMaxStyle) {
		    break;
		}
#ifndef notdef
		/* if old version of mklinux then skip CD drive */
		if (a->style == kSCSI_Disks && a->index == 3) {
		    a->index += 1;
		}
#endif
		/* generate result */
		result = (char *) malloc(20);
		if (result != NULL) {
		    /*
		     * for DR3 we should actually iterate through:
		     *
		     *    /dev/sd[a...]    # first missing is end of list
		     *    /dev/hd[a...]    # may be holes in sequence
		     *    /dev/scd[0...]   # first missing is end of list
		     *
		     * and stop in each group when either a stat of
		     * the name fails or if an open fails for
		     * particular reasons.
		     */
		    bump = 0;
		    value = (int) a->index;
		    switch (a->style) {
		    case kSCSI_Disks:
			if (value < 26) {
			    snprintf(result, 20, "/dev/sd%c", 'a'+value);
			} else if (value < 676) {
			    snprintf(result, 20, "/dev/sd%c%c",
				    'a' + value / 26,
				    'a' + value % 26);
			} else {
			    bump = -1;
			}
			break;
		    case kATA_Devices:
			if (value < 26) {
			    snprintf(result, 20, "/dev/hd%c", 'a'+value);
			} else {
			    bump = -1;
			}
			break;
		    case kSCSI_CDs:
			if (value < 10) {
			    snprintf(result, 20, "/dev/scd%c", '0'+value);
			} else {
			    bump = -1;
			}
			break;
		    }
		    if (bump != 0) {
			// already set don't even check
		    } else if (stat(result, &info) < 0) {
			bump = 1;
		    } else if ((fd = open(result, O_RDONLY)) >= 0) {
			close(fd);
#if defined(__linux__) || defined(__unix__)
		    } else if (errno == ENXIO || errno == ENODEV) {
			if (a->style == kATA_Devices) {
			    bump = -1;
			} else {
			    bump = 1;
			}
#endif
		    }
		    if (bump) {
			if (bump > 0) {
			    a->style += 1; /* next style */
			    a->index = 0; /* first index again */
			} else {
			    a->index += 1; /* next index */
			}
			free(result);
			continue;
		    }
		}

		a->index += 1; /* next index */
		return result;
	    }
	    a->m.state = kEnd;
	    /* fall through to end */
	case kEnd:
	default:
	    break;
	}
    }
    return 0 /* no entry */;
}


void
delete_file_iterator(MEDIA_ITERATOR m)
{
    return;
}
