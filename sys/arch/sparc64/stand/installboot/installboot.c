/*	$OpenBSD: installboot.c,v 1.12 2011/12/28 13:53:23 jsing Exp $	*/
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

int	verbose, nowrite;
char	*boot, *proto, *dev;

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
	int c, devfd, protofd;
	char *protostore;
	size_t protosize;
	size_t blanklen;
	struct stat sb;

	while ((c = getopt(argc, argv, "nv")) != -1) {
		switch (c) {
		case 'n':
			/* Do not actually write the bootblock to disk */
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

	proto = argv[optind++];
	dev = argv[optind];

	if (verbose) {
		printf("proto: %s\n", proto);
		printf("device: %s\n", dev);
	}

	if ((protofd = open(proto, O_RDONLY)) < 0)
		err(1, "open: %s", proto);

	if (fstat(protofd, &sb) == -1)
		err(1, "fstat: %s", proto);
	if (sb.st_size == 0)
		errx(1, "%s is empty", proto);

	/* there must be a better way */
	blanklen = DEV_BSIZE - ((sb.st_size + DEV_BSIZE) & (DEV_BSIZE - 1));
	protosize = sb.st_size + blanklen;
	if ((protostore = mmap(0, protosize, PROT_READ|PROT_WRITE, MAP_PRIVATE,
	    protofd, 0)) == MAP_FAILED)
		err(1, "mmap: %s", proto);
	/* and provide the rest of the block */
	if (blanklen)
		memset(protostore + sb.st_size, 0, blanklen);

	if (nowrite)
		return 0;

	/* Write patched proto bootblocks into the superblock */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if ((devfd = open(dev, O_RDWR, 0)) < 0)
		err(1, "open: %s", dev);

	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek bootstrap");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();

	if (write(devfd, protostore, protosize) != protosize)
		err(1, "write bootstrap");

	close(devfd);

	return 0;
}
