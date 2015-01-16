/*	$OpenBSD: sparc64_installboot.c,v 1.3 2015/01/16 00:05:12 deraadt Exp $	*/

/*
 * Copyright (c) 2012, 2013 Joel Sing <jsing@openbsd.org>
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

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ufs/ffs/fs.h>

#include "installboot.h"

char	*bootldr;

char	*blkstore;
char	*ldrstore;
size_t	blksize;
size_t	ldrsize;

void
md_init(void)
{
	stages = 2;
	stage1 = "/usr/mdec/bootblk";
	stage2 = "/usr/mdec/ofwboot";

	bootldr = "/ofwboot";
}

void
md_loadboot(void)
{
	struct stat sb;
	size_t blocks;
	int fd;

	/* Load first-stage boot block. */
	if ((fd = open(stage1, O_RDONLY)) < 0)
		err(1, "open");
	if (fstat(fd, &sb) == -1)
		err(1, "fstat");
	blocks = howmany((size_t)sb.st_size, DEV_BSIZE);
	blksize = blocks * DEV_BSIZE;
	if (verbose)
		fprintf(stderr, "boot block is %zu bytes "
                    "(%zu blocks @ %u bytes = %zu bytes)\n",
                    (ssize_t)sb.st_size, blocks, DEV_BSIZE, blksize);
	if (blksize > SBSIZE - DEV_BSIZE)
		errx(1, "boot blocks too big (%zu > %d)",
		    blksize, SBSIZE - DEV_BSIZE);
	blkstore = calloc(1, blksize);
	if (blkstore == NULL)
		err(1, "calloc");
	if (read(fd, blkstore, sb.st_size) != (ssize_t)sb.st_size)
		err(1, "read");
	close(fd);

	/* Load second-stage boot loader. */
        if ((fd = open(stage2, O_RDONLY)) < 0)
                err(1, "open");
        if (fstat(fd, &sb) == -1)
                err(1, "stat");
        ldrsize = sb.st_size;
        ldrstore = malloc(ldrsize);
        if (ldrstore == NULL)
                err(1, "malloc");
        if (read(fd, ldrstore, ldrsize) != (ssize_t)sb.st_size)
                err(1, "read");
        close(fd);
}

void
md_installboot(int devfd, char *dev)
{
	/* XXX - is this necessary? */
	sync();

	bootldr = fileprefix(root, bootldr);
	if (!nowrite)
		filecopy(stage2, bootldr);

	/* Write bootblock into the superblock. */
	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek");
	if (verbose)
		fprintf(stderr, "%s boot block to disk %s\n",
		    (nowrite ? "would write" : "writing"), dev);
	if (nowrite)
		return;
	if (write(devfd, blkstore, blksize) != (ssize_t)blksize)
		err(1, "write");
}
