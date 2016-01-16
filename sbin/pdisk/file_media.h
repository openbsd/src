/*	$OpenBSD: file_media.h,v 1.7 2016/01/16 22:28:14 krw Exp $	*/

/*
 * file_media.h -
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

#ifndef __file_media__
#define __file_media__


/*
 * Defines
 */


/*
 * Types
 */
typedef struct file_media *FILE_MEDIA;

struct file_media {
    long long	size_in_bytes;  /* offset granularity */
    int		fd;
    int		regular_file;
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
FILE_MEDIA open_file_as_media(char *file, int oflag);
long read_file_media(FILE_MEDIA m, long long offset, unsigned long count, void *address);
long write_file_media(FILE_MEDIA m, long long offset, unsigned long count, void *address);
long close_file_media(FILE_MEDIA m);
long os_reload_file_media(FILE_MEDIA m);

#endif /* __file_media__ */
