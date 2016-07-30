/*	$OpenBSD: makeinodes.c,v 1.1 2016/07/30 10:56:13 schwarze Exp $ */
/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define	HSIZE	 64

int
main(void)
{
	struct stat	 sb1, sb2;
	long long	 diff;
	int		 fd;

	if (mkdir("man", 0755) == -1)
		err(1, "mkdir(man)");
	if (chdir("man") == -1)
		err(1, "chdir(man)");
	if (mkdir("man1", 0755) == -1)
		err(1, "mkdir(man1)");
	if (chdir("man1") == -1)
		err(1, "chdir(man1)");
	if ((fd = open("1", O_WRONLY | O_CREAT | O_EXCL, 0644)) == -1)
		err(1, "open(1)");
	if (fstat(fd, &sb1) == -1)
		err(1, "fstat(1)");
	if (close(fd) == -1)
		err(1, "close(1)");
	if ((fd = open("2", O_WRONLY | O_CREAT | O_EXCL, 0644)) == -1)
		err(1, "open(2)");
	if (fstat(fd, &sb2) == -1)
		err(1, "fstat(2)");
	if (close(fd) == -1)
		err(1, "close(2)");
	while ((diff = sb2.st_ino % HSIZE - sb1.st_ino % HSIZE) == 0) {
		if ((fd = open("3", O_WRONLY | O_CREAT | O_EXCL, 0644)) == -1)
			err(1, "open(3)");
		if (fstat(fd, &sb2) == -1)
			err(1, "fstat(3)");
		if (close(fd) == -1)
			err(1, "close(3)");
		if (rename("3", "2") == -1)
			err(1, "rename(3, 2)");
	}
	if (diff < 0) {
		if (rename("1", "3") == -1)
			err(1, "rename(1, 3)");
		if (rename("2", "1") == -1)
			err(1, "rename(2, 1)");
		if (rename("3", "2") == -1)
			err(1, "rename(3, 2)");
	}
	return 0;
}
