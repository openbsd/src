/*	$OpenBSD: file_media.c,v 1.34 2016/01/18 16:41:41 krw Exp $	*/

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
#include <sys/dkio.h>
#include <sys/disklabel.h>
#include <err.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <util.h>

#include "file_media.h"

void		compute_block_size(int, char *);
void		file_init(void);

void
compute_block_size(int fd, char *name)
{
	struct disklabel dl;
	struct stat st;

	if (fstat(fd, &st) == -1)
		err(1, "can't fstat %s", name);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", name);
	if (ioctl(fd, DIOCGPDINFO, &dl) == -1)
		err(1, "can't get disklabel for %s", name);

	if (dl.d_secsize != DEV_BSIZE)
		err(1, "%u-byte sector size not supported", dl.d_secsize);
}


struct file_media *
open_file_as_media(char *file, int oflag)
{
	struct stat info;
	struct file_media *a;
	int fd;

	a = 0;
	fd = opendev(file, oflag, OPENDEV_PART, NULL);
	if (fd >= 0) {
		a = malloc(sizeof(struct file_media));
		if (a != 0) {
			compute_block_size(fd, file);
			a->fd = fd;
			if (fstat(fd, &info) < 0) {
				warn("can't stat file '%s'", file);
			}
		} else {
			close(fd);
		}
	}
	return (a);
}


long
read_file_media(struct file_media *a, long long offset, unsigned long count,
		void *address)
{
	ssize_t off;

	off = pread(a->fd, address, count, offset);
	if (off == count)
		return (1);

	if (off == 0)
		fprintf(stderr, "end of file encountered");
	else if (off == -1)
		warn("reading file failed");
	else
		fprintf(stderr, "short read");

	return (0);
}


long
write_file_media(struct file_media *a, long long offset, unsigned long count,
		 void *address)
{
	ssize_t off;

	off = pwrite(a->fd, address, count, offset);
	if (off == count)
		return (1);

	warn("writing to file failed");
	return (0);
}


long
close_file_media(struct file_media * a)
{
	if (a == 0) {
		return 0;
	}
	close(a->fd);
	return 1;
}
