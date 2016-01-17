/*	$OpenBSD: file_media.h,v 1.10 2016/01/17 19:39:20 krw Exp $	*/

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


struct file_media {
    long long	size_in_bytes;  /* offset granularity */
    int		fd;
    int		regular_file;
};

struct file_media *open_file_as_media(char *, int);
long read_file_media(struct file_media *, long long, unsigned long, void *);
long write_file_media(struct file_media *, long long, unsigned long, void *);
long close_file_media(struct file_media *m);
long os_reload_file_media(struct file_media *m);

#endif /* __file_media__ */
