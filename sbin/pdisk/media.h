/*	$OpenBSD: media.h,v 1.11 2016/01/16 21:59:30 krw Exp $	*/

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

struct media {
    long long       size_in_bytes;  /* offset granularity */
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

#endif /* __media__ */
