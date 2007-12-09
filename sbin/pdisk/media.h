/*
 * media.h -
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

#ifndef __media__
#define __media__


/*
 * Media is an abstraction of a disk device.
 *
 * A media object has the following visible attributes:
 *
 *      a granularity       (e.g. 512, 1024, 1, etc.)
 *      a total size in bytes
 *
 *  And the following operations are available:
 *
 *      open
 *      read @ byte offset for size in bytes
 *      write @ byte offset for size in bytes
 *      close
 *
 * XXX Should really split public media interface from "protected" interface.
 */


/*
 * Defines
 */


/*
 * Types
 */
/* those whose use media objects need just the pointer type */
typedef struct media *MEDIA;

/* those who define media objects need the struct and internal routine types */
typedef long (*media_read)(MEDIA m, long long offset, unsigned long count, void *address);
typedef long (*media_write)(MEDIA m, long long offset, unsigned long count, void *address);
typedef long (*media_close)(MEDIA m);
typedef long (*media_os_reload)(MEDIA m);

struct media {
    long            kind;           /* kind of media - SCSI, IDE, etc. */
    unsigned long   grain;          /* granularity (offset & size) */
    long long       size_in_bytes;  /* offset granularity */
    media_read      do_read;        /* device specific routines */
    media_write     do_write;
    media_close     do_close;
    media_os_reload do_os_reload;
				    /* specific media kinds will add extra info */
};

/* those whose use media object iterators need just the pointer type */
typedef struct media_iterator *MEDIA_ITERATOR;

/* those who define media object iterators need the struct and internal routine types */
typedef void (*media_iterator_reset)(MEDIA_ITERATOR m);
typedef char* (*media_iterator_step)(MEDIA_ITERATOR m);
typedef void (*media_iterator_delete)(MEDIA_ITERATOR m);

typedef enum {
    kInit,
    kReset,
    kIterating,
    kEnd
} media_iterator_state;

struct media_iterator {
    long                    kind;           /* kind of media - SCSI, IDE, etc. */
    media_iterator_state    state;          /* init, reset, iterating, at_end */
    media_iterator_reset    do_reset;       /* device specific routines */
    media_iterator_step     do_step;
    media_iterator_delete   do_delete;
					    /* specific media kinds will add extra info */
};


/*
 * Global Constants
 */


/*
 * Global Variables
 */


/*
 * Forward declarations
 */
/* those whose use media objects need these routines */
unsigned long media_granularity(MEDIA m);
long long media_total_size(MEDIA m);
long read_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_media(MEDIA m, long long offset, unsigned long count, void *address);
void close_media(MEDIA m);
void os_reload_media(MEDIA m);

/* those who define media objects need these routines also */
long allocate_media_kind(void);
MEDIA new_media(long size);
void delete_media(MEDIA m);

/* those who define media object iterators need these routines also */
MEDIA_ITERATOR new_media_iterator(long size);

#endif /* __media__ */
