/*
 * media.c -
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

#include "media.h"


/*
 * Defines
 */


/*
 * Types
 */


/*
 * Global Constants
 */


/*
 * Global Variables
 */
static long media_kind = 0;

/*
 * Forward declarations
 */


/*
 * Routines
 */
long
allocate_media_kind(void)
{
    media_kind++;
    return media_kind;
}


MEDIA
new_media(long size)
{
    return (MEDIA) malloc(size);
}


void
delete_media(MEDIA m)
{
    if (m == 0) {
	return;
    }
    free(m);
}


unsigned long
media_granularity(MEDIA m)
{
    if (m == 0) {
	return 0;
    } else {
	return m->grain;
    }
}


long long
media_total_size(MEDIA m)
{
    if (m == 0) {
	return 0;
    } else {
	return m->size_in_bytes;
    }
}


long
read_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    long result;

    if (m != 0 && m->do_read != 0) {
	//printf("media: read type %d, offset %Ld, count %d\n\t", m->kind, offset, count);
	result = (*m->do_read)(m, offset, count, address);
	//printf(" - returns %d\n", result);
	return result;
    } else {
	return 0;
    }
}


long
write_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    long result;

    if (m != 0 && m->do_write != 0) {
	//printf("media: write type %d, offset %Ld, count %d\n\t", m->kind, offset, count);
	result = (*m->do_write)(m, offset, count, address);
	//printf(" - returns %d\n", result);
	return result;
    } else {
	return 0;
    }
}


void
close_media(MEDIA m)
{
    if (m == 0) {
	return;
    }
    if (m->kind != 0) {
	if (m->do_close != 0) {
	    (*m->do_close)(m);
	}
	m->kind = 0;
	delete_media(m);
    }
}


void
os_reload_media(MEDIA m)
{
    if (m != 0 && m->do_os_reload != 0) {
	(*m->do_os_reload)(m);
    }
}


MEDIA_ITERATOR
new_media_iterator(long size)
{
    return (MEDIA_ITERATOR) malloc(size);
}
