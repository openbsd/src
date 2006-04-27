/*
 * deblock_media.c -
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


// for malloc() & free()
#include <stdlib.h>
// for memcpy()
#include <string.h>

#include "deblock_media.h"


/*
 * Defines
 */


/*
 * Types
 */
typedef struct deblock_media *DEBLOCK_MEDIA;

struct deblock_media {
    struct media    m;
    long            need_filtering;
    MEDIA           next_media;
    unsigned long   next_block_size;
    unsigned char   *buffer;
};

struct deblock_globals {
    long        exists;
    long        kind;
};


/*
 * Global Constants
 */


/*
 * Global Variables
 */
static long deblock_inited = 0;
static struct deblock_globals deblock_info;

/*
 * Forward declarations
 */
void deblock_init(void);
DEBLOCK_MEDIA new_deblock_media(void);
long read_deblock_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_deblock_media(MEDIA m, long long offset, unsigned long count, void *address);
long close_deblock_media(MEDIA m);
long os_reload_deblock_media(MEDIA m);


/*
 * Routines
 */
void
deblock_init(void)
{
    if (deblock_inited != 0) {
	return;
    }
    deblock_inited = 1;

    deblock_info.kind = allocate_media_kind();
}


DEBLOCK_MEDIA
new_deblock_media(void)
{
    return (DEBLOCK_MEDIA) new_media(sizeof(struct deblock_media));
}


MEDIA
open_deblock_media(long new_block_size, MEDIA m)
{
    DEBLOCK_MEDIA   a;
    unsigned long   block_size;

    if (deblock_inited == 0) {
	deblock_init();
    }

    a = 0;
    if (m != 0) {
	block_size = media_granularity(m);

	if (new_block_size == block_size) {
	    return m;

	} else if (new_block_size > block_size) {
	    if ((new_block_size % block_size) == 0) {
		/* no filtering necessary */
		a = new_deblock_media();
		if (a != 0) {
		    a->need_filtering = 0;
		    a->next_block_size = block_size;
		    a->buffer = 0;
		}
	    } else {
		/* too hard to bother with */
	    }
	} else /* new_block_size < block_size */ {
	    if ((block_size % new_block_size) == 0) {
		/* block & unblock */
		a = new_deblock_media();
		if (a != 0) {
		    a->need_filtering = 1;
		    a->next_block_size = block_size;
		    a->buffer = malloc(block_size);
		}
	    } else {
		/* too hard to bother with */
	    }
	}
	if (a != 0) {
	    a->m.kind = deblock_info.kind;
	    a->m.grain = new_block_size;
	    a->m.size_in_bytes = media_total_size(m);
	    a->m.do_read = read_deblock_media;
	    a->m.do_write = write_deblock_media;
	    a->m.do_close = close_deblock_media;
	    a->m.do_os_reload = os_reload_deblock_media;
	    a->next_media = m;
	}
    }
    return (MEDIA) a;
}


long
read_deblock_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    DEBLOCK_MEDIA a;
    long rtn_value;
    unsigned long next_size;
    unsigned long partial_offset;
    unsigned long partial_count;
    long long cur_offset;
    unsigned long remainder;
    unsigned char *addr;

    a = (DEBLOCK_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != deblock_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (a->need_filtering == 0) {
	rtn_value = read_media(a->next_media, offset, count, address);
    } else {
	next_size = a->next_block_size;
	addr = address;
	cur_offset = offset;
	remainder = count;
	rtn_value = 1;

	/* read partial */
	partial_offset = cur_offset % next_size;
	if (partial_offset != 0) {
	    partial_count = next_size - partial_offset;
	    if (partial_count > remainder) {
		partial_count = remainder;
	    }
	    rtn_value = read_media(a->next_media, cur_offset - partial_offset, next_size, a->buffer);
	    if (rtn_value != 0) {
		memcpy (addr, a->buffer + partial_offset, partial_count);
		addr += partial_count;
		cur_offset += partial_count;
		remainder -= partial_count;
	    }
	}
	/* read fulls as long as needed */
	if (rtn_value != 0 && remainder > next_size) {
	    partial_count = remainder - (remainder % next_size);
	    rtn_value = read_media(a->next_media, cur_offset, partial_count, addr);
	    addr += partial_count;
	    cur_offset += partial_count;
	    remainder -= partial_count;
	}
	/* read partial */
	if (rtn_value != 0 && remainder > 0) {
	    partial_count = remainder;
	    rtn_value = read_media(a->next_media, cur_offset, next_size, a->buffer);
	    if (rtn_value != 0) {
		memcpy (addr, a->buffer, partial_count);
	    }
	}
    }
    return rtn_value;
}


long
write_deblock_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    DEBLOCK_MEDIA a;
    long rtn_value;
    unsigned long next_size;
    unsigned long partial_offset;
    unsigned long partial_count;
    long long cur_offset;
    unsigned long remainder;
    unsigned char *addr;

    a = (DEBLOCK_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != deblock_info.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (a->need_filtering == 0) {
	rtn_value = write_media(a->next_media, offset, count, address);
    } else {
	next_size = a->next_block_size;
	addr = address;
	cur_offset = offset;
	remainder = count;
	rtn_value = 1;

	/* write partial */
	partial_offset = cur_offset % next_size;
	if (partial_offset != 0) {
	    partial_count = next_size - partial_offset;
	    if (partial_count > remainder) {
		partial_count = remainder;
	    }
	    rtn_value = read_media(a->next_media, cur_offset - partial_offset, next_size, a->buffer);
	    if (rtn_value != 0) {
		memcpy (a->buffer + partial_offset, addr, partial_count);
		rtn_value = write_media(a->next_media, cur_offset - partial_offset, next_size, a->buffer);
		addr += partial_count;
		cur_offset += partial_count;
		remainder -= partial_count;
	    }
	}
	/* write fulls as long as needed */
	if (rtn_value != 0 && remainder > next_size) {
	    partial_count = remainder - (remainder % next_size);
	    rtn_value = write_media(a->next_media, cur_offset, partial_count, addr);
	    addr += partial_count;
	    cur_offset += partial_count;
	    remainder -= partial_count;
	}
	/* write partial */
	if (rtn_value != 0 && remainder > 0) {
	    partial_count = remainder;
	    rtn_value = read_media(a->next_media, cur_offset, next_size, a->buffer);
	    if (rtn_value != 0) {
		memcpy (a->buffer, addr, partial_count);
		rtn_value = write_media(a->next_media, cur_offset, next_size, a->buffer);
	    }
	}
    }
    /* recompute size to handle file media */
    a->m.size_in_bytes = media_total_size(a->next_media);
    return rtn_value;
}


long
close_deblock_media(MEDIA m)
{
    DEBLOCK_MEDIA a;

    a = (DEBLOCK_MEDIA) m;
    if (a == 0) {
	return 0;
    } else if (a->m.kind != deblock_info.kind) {
	/* XXX need to error here - this is an internal problem */
	return 0;
    }

    close_media(a->next_media);
    free(a->buffer);
    return 1;
}


long
os_reload_deblock_media(MEDIA m)
{
    DEBLOCK_MEDIA a;

    a = (DEBLOCK_MEDIA) m;
    if (a == 0) {
	return 0;
    } else if (a->m.kind != deblock_info.kind) {
	/* XXX need to error here - this is an internal problem */
	return 0;
    }

    os_reload_media(a->next_media);
    return 1;
}
