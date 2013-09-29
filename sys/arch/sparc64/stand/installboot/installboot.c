/*	$OpenBSD: installboot.c,v 1.17 2013/09/29 21:30:50 jmc Exp $	*/
/*	$NetBSD: installboot.c,v 1.8 2001/02/19 22:48:59 cgd Exp $ */

/*-
 * Copyright (c) 2012 Joel Sing <jsing@openbsd.org>
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
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dev/biovar.h>
#include <dev/softraidvar.h>

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
size_t	blksize;

static void	usage(void);
int 		main(int, char *[]);
static void	write_bootblk(int);

static int	sr_volume(int, int *, int *);
static void	sr_installboot(int);
static void	sr_install_bootblk(int, int, int);

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-nv] bootblk device\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char		*blkfile, *realdev;
	int		vol = -1, ndisks = 0, disk;
	int		c, devfd, blkfd;
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

	if ((devfd = opendev(dev, (nowrite ? O_RDONLY : O_RDWR),
	    OPENDEV_PART, &realdev)) < 0)
		err(1, "open: %s", realdev);
	if (verbose)
		printf("device: %s\n", realdev);

	if (sr_volume(devfd, &vol, &ndisks)) {

		/* Install boot loader in softraid volume. */
		sr_installboot(devfd);

		/* Install bootblk on each disk that is part of this volume. */
		for (disk = 0; disk < ndisks; disk++)
			sr_install_bootblk(devfd, vol, disk);

	} else {

		/* Write boot blocks to device. */
		write_bootblk(devfd);

	}

	close(devfd);
}

static void
write_bootblk(int devfd)
{
	/*
	 * Write bootblock into the superblock.
	 */

	if (nowrite)
		return;

	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek boot block");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();

	if (write(devfd, blkstore, blksize) != blksize)
		err(1, "write boot block");
}

static int
sr_volume(int devfd, int *vol, int *disks)
{
	struct	bioc_inq bi;
	struct	bioc_vol bv;
	int	rv, i;

	/* Get volume information. */
	memset(&bi, 0, sizeof(bi));
	rv = ioctl(devfd, BIOCINQ, &bi);
	if (rv == -1)
		return 0;

	/* XXX - softraid volumes will always have a "softraid0" controller. */
	if (strncmp(bi.bi_dev, "softraid0", sizeof("softraid0")))
		return 0;

	/* Locate specific softraid volume. */
	for (i = 0; i < bi.bi_novol; i++) {

		memset(&bv, 0, sizeof(bv));
		bv.bv_volid = i;
		rv = ioctl(devfd, BIOCVOL, &bv);
		if (rv == -1)
			err(1, "BIOCVOL");

		if (strncmp(dev, bv.bv_dev, sizeof(bv.bv_dev)) == 0) {
			*vol = i;
			*disks = bv.bv_nodisk;
			break;
		}

	}

	if (verbose)
		fprintf(stderr, "%s: softraid volume with %i disk(s)\n",
		    dev, *disks);

	return 1;
}

static void
sr_installboot(int devfd)
{
	struct bioc_installboot bb;
	struct stat sb;
	int fd, i, rv;
	u_char *p;

	/*
	 * Install boot loader into softraid boot loader storage area.
	 */
	bb.bb_bootldr = "XXX";
	bb.bb_bootldr_size = sizeof("XXX");
	bb.bb_bootblk = blkstore;
	bb.bb_bootblk_size = blksize;
	strncpy(bb.bb_dev, dev, sizeof(bb.bb_dev));
	if (!nowrite) {
		if (verbose)
			fprintf(stderr, "%s: installing boot loader on "
			    "softraid volume\n", dev);
		rv = ioctl(devfd, BIOCINSTALLBOOT, &bb);
		if (rv != 0)
			errx(1, "softraid installboot failed");
	}
}

static void
sr_install_bootblk(int devfd, int vol, int disk)
{
	struct bioc_disk bd;
	struct disklabel dl;
	struct partition *pp;
	uint32_t poffset;
	char *realdev;
	char part;
	int diskfd;
	int rv;

	/* Get device name for this disk/chunk. */
	memset(&bd, 0, sizeof(bd));
	bd.bd_volid = vol;
	bd.bd_diskid = disk;
	rv = ioctl(devfd, BIOCDISK, &bd);
	if (rv == -1)
		err(1, "BIOCDISK");

	/* Check disk status. */
	if (bd.bd_status != BIOC_SDONLINE && bd.bd_status != BIOC_SDREBUILD) {
		fprintf(stderr, "softraid chunk %u not online - skipping...\n",
		    disk);
		return;	
	}

	if (strlen(bd.bd_vendor) < 1)
		errx(1, "invalid disk name");
	part = bd.bd_vendor[strlen(bd.bd_vendor) - 1];
	if (part < 'a' || part >= 'a' + MAXPARTITIONS)
		errx(1, "invalid partition %c\n", part);
	bd.bd_vendor[strlen(bd.bd_vendor) - 1] = '\0';

	/* Open device. */
	if ((diskfd = opendev(bd.bd_vendor, (nowrite ? O_RDONLY : O_RDWR),
	    OPENDEV_PART, &realdev)) < 0)
		err(1, "open: %s", realdev);

	if (verbose)
		fprintf(stderr, "%s%c: installing boot blocks on %s\n",
		    bd.bd_vendor, part, realdev);

	/* Write boot blocks to device. */
	write_bootblk(diskfd);

	close(diskfd);
}
