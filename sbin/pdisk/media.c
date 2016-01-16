/*	$OpenBSD: media.c,v 1.10 2016/01/16 21:29:07 krw Exp $	*/

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

/*
 * Forward declarations
 */


/*
 * Routines
 */


MEDIA
new_media(long size)
{
    return malloc(size);
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
