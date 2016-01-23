/*	$OpenBSD: file_media.c,v 1.39 2016/01/23 03:46:18 krw Exp $	*/

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

#include <sys/param.h>		/* DEV_BSIZE */

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "file_media.h"

int
read_block(int fd, uint64_t sector, void *address)
{
	ssize_t off;

	off = pread(fd, address, DEV_BSIZE, sector * DEV_BSIZE);
	if (off == DEV_BSIZE)
		return (1);

	if (off == 0)
		fprintf(stderr, "end of file encountered");
	else if (off == -1)
		warn("reading file failed");
	else
		fprintf(stderr, "short read");

	return (0);
}

int
write_block(int fd, uint64_t sector, void *address)
{
	ssize_t off;

	off = pwrite(fd, address, DEV_BSIZE, sector * DEV_BSIZE);
	if (off == DEV_BSIZE)
		return (1);

	warn("writing to file failed");
	return (0);
}
