/*	$OpenBSD: fsirand.c,v 1.2 1997/01/27 23:21:57 millert Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint                                                              
static char rcsid[] = "$OpenBSD: fsirand.c,v 1.2 1997/01/27 23:21:57 millert Exp $";
#endif /* not lint */                                                        

#include <sys/param.h>
#include <sys/types.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define	INOBUFSIZE	56*1024	/* size of buffer to read inodes (from fsck) */

void fsirand __P((int, struct fs *, int));
void usage __P((int));

extern char *__progname;

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	struct fs *sblock;
	int devfd, n, force = 0, printonly = 0;
	char sbuf[SBSIZE], *devpath;

	while ((n = getopt(argc, argv, "fp")) != -1) {
		switch (n) {
		case 'p':
			printonly++;
			break;
		case 'f':
			force++;
			break;
		default:
			usage(1);
		}
	}
	if (argc - optind != 1)
		usage(1);

	/* Open device and read in superblock */
	if ((devfd = opendev(argv[optind], printonly ? O_RDONLY : O_RDWR,
	     OPENDEV_PART, &devpath)) < 0)
		err(1, "Can't open %s", devpath);
	(void)memset(&sbuf, 0, sizeof(sbuf));
	sblock = (struct fs *)&sbuf;
	if (lseek(devfd, SBOFF, SEEK_SET) == -1)
		err(1, "Can't seek to superblock (%qd) on %s", SBOFF, devpath);
	if ((n = read(devfd, (void *)sblock, SBSIZE)) != SBSIZE)
		err(1, "Can't read superblock on %s: %s", devpath,
		    (n < SBSIZE) ? "short read" : strerror(errno));

	/* Simple sanity checks on the superblock */
	if (sblock->fs_magic != FS_MAGIC)
		errx(1, "Wrong magic number in superblock");
	if (sblock->fs_sbsize > SBSIZE)
		errx(1, "Superblock size is preposterous");
	if (!force && !printonly && sblock->fs_clean != FS_ISCLEAN)
		errx(1, "Filesystem is not clean, fsck %s before running %s\n",
		     devpath, __progname);

	/* Do the work */
	fsirand(devfd, sblock, printonly);

	(void)close(devfd);

	exit(0);
}

void
fsirand(fd, sblock, printonly)
	int fd;
	struct fs *sblock;
	int printonly;
{
	size_t size, n;
	daddr_t dblk;
	static struct dinode *dp;
	ino_t inumber, lastinum, maxino;
	size_t readcnt, readpercg, fullcnt, inobufsize, partialcnt, partialsize;
	struct dinode *inodebuf;

	/* Initialize variables and allocate buffer */
	inumber = 0;
	lastinum = 0;
	readcnt = 0;
	maxino = sblock->fs_ncg * sblock->fs_ipg;
	inobufsize = blkroundup(sblock, INOBUFSIZE);
	fullcnt = inobufsize / sizeof(struct dinode);
	readpercg = sblock->fs_ipg / fullcnt;
	partialcnt = sblock->fs_ipg % fullcnt;
	partialsize = partialcnt * sizeof(struct dinode);
	if (partialcnt != 0) {
		readpercg++;
	} else {
		partialcnt = fullcnt;
		partialsize = inobufsize;
	}
	if ((inodebuf = (struct dinode *)malloc((unsigned)inobufsize)) == NULL)
		errx(1, "Can't allocate memory for inode buffer");

	/* Grab inodes a buffer's length at a time */
	while (lastinum < maxino) {
		readcnt++;
		dblk = fsbtodb(sblock, ino_to_fsba(sblock, lastinum));
		if (readcnt % readpercg == 0) {
			size = partialsize;
			lastinum += partialcnt;
		} else {
			size = inobufsize;
			lastinum += fullcnt;
		}
		/* XXX - don't use DEV_BSIZE, get value from disklabel! */
		if (lseek(fd, (off_t)(dblk * DEV_BSIZE), SEEK_SET) < 0)
			err(1, "Can't seek to %qd", (off_t)(dblk * DEV_BSIZE));
		else if ((n = read(fd, inodebuf, size)) != size)
			errx(1, "Can't read inodes: %s",
			     (n < size) ? "short read" : strerror(errno));

		for (dp = inodebuf, n = 0; inumber < lastinum; n++, inumber++)
			if (inumber >= ROOTINO) {
				if (printonly)
					(void)printf("ino %d gen %x\n",
					    inumber, dp[n].di_gen);
				else
					dp[n].di_gen = arc4random();
			}
		if (!printonly) {
			/* XXX - don't use DEV_BSIZE, get from disklabel! */
			if (lseek(fd, (off_t)(dblk * DEV_BSIZE), SEEK_SET) < 0)
				err(1, "Can't seek to %qd",
				    (off_t)(dblk * DEV_BSIZE));
			else if ((n = write(fd, inodebuf, size)) != size)
				errx(1, "Can't write inodes: %s",
				     (n != size) ? "short write" :
				     strerror(errno));
		}
	}
	(void)free(inodebuf);
}

void
usage(ex)
	int ex;
{
	(void)fprintf(stderr, "Usage: %s [ -p ] special\n", __progname);
	exit(ex);
}
