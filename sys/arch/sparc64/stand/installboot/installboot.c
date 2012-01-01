/*	$OpenBSD: installboot.c,v 1.13 2012/01/01 16:11:13 jsing Exp $	*/
/*	$NetBSD: installboot.c,v 1.8 2001/02/19 22:48:59 cgd Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ufs/ffs/fs.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

int	verbose, nowrite;
char	*dev, *blkstore;

static void	usage(void);
int 		main(int, char *[]);

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s [-nv] <bootblk> <device>\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char		*blkfile, *realdev;
	int		c, devfd, blkfd;
	size_t		blksize;
	struct stat	sb;

	while ((c = getopt(argc, argv, "nv")) != -1) {
		switch (c) {
		case 'n':
			/* Do not actually write the bootblock to disk. */
			nowrite = 1;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind < 2)
		usage();

	blkfile = argv[optind++];
	dev = argv[optind];

	if (verbose)
		printf("bootblk: %s\n", blkfile);

	if ((blkfd = open(blkfile, O_RDONLY)) < 0)
		err(1, "open: %s", blkfile);

	if (fstat(blkfd, &sb) == -1)
		err(1, "fstat: %s", blkfile);
	if (sb.st_size == 0)
		errx(1, "%s is empty", blkfile);

	blksize = howmany(sb.st_size, DEV_BSIZE) * DEV_BSIZE;
	if (blksize > SBSIZE - DEV_BSIZE)
		errx(1, "boot blocks too big");
	if ((blkstore = malloc(blksize)) == NULL)
		err(1, "malloc: %s", blkfile);
	bzero(blkstore, blksize);
	if (read(blkfd, blkstore, sb.st_size) != sb.st_size)
		err(1, "read: %s", blkfile);

	if (nowrite)
		return 0;

	/* Write boot blocks into the superblock. */
	if ((devfd = opendev(dev, (nowrite ? O_RDONLY : O_RDWR),
	    OPENDEV_PART, &realdev)) < 0)
		err(1, "open: %s", realdev);
	if (verbose)
		printf("device: %s\n", realdev);
	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek boot block");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();

	if (write(devfd, blkstore, blksize) != blksize)
		err(1, "write boot block");

	close(devfd);

	return 0;
}
