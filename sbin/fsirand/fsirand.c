/*	$OpenBSD: fsirand.c,v 1.3 1997/01/28 04:14:55 millert Exp $	*/

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
static char rcsid[] = "$OpenBSD: fsirand.c,v 1.3 1997/01/28 04:14:55 millert Exp $";
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

void usage __P((int));
void fsirand __P((char *));

extern char *__progname;

int printonly = 0, force = 0;

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int n;

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
	if (argc - optind < 1)
		usage(1);

	for (n = optind; n < argc; n++) {
		fsirand(argv[n]);
		if (n < argc - 1)
			putchar('\n');
	}

	exit(0);
}

void
fsirand(device)
	char *device;
{
	static struct dinode *inodebuf;
	static size_t oldibufsize;
	size_t ibufsize;
	struct fs *sblock;
	ino_t inumber, maxino;
	static daddr_t dblk;
	char sbuf[SBSIZE];
	int devfd, n;
	char *devpath;

	/* Open device and read in superblock */
	if ((devfd = opendev(device, printonly ? O_RDONLY : O_RDWR,
	     OPENDEV_PART, &devpath)) < 0)
		err(1, "Can't open %s", devpath);
	(void)puts(devpath);

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

	maxino = sblock->fs_ncg * sblock->fs_ipg;
	ibufsize = sizeof(struct dinode) * sblock->fs_ipg;
	if (oldibufsize < ibufsize) {
		if ((inodebuf = realloc(inodebuf, ibufsize)) == NULL)
			errx(1, "Can't allocate memory for inode buffer");
		oldibufsize = ibufsize;
	}

	/* Randomize inodes a cylinder group at a time */
	for (inumber = 0; inumber < maxino;) {
		/* Read in inodes, then print or randomize generation nums */
		dblk = fsbtodb(sblock, ino_to_fsba(sblock, inumber));
		/* XXX - don't use DEV_BSIZE, get value from disklabel! */
		if (lseek(devfd, (off_t)(dblk * DEV_BSIZE), SEEK_SET) < 0) {
			warn("Can't seek to %qd", (off_t)(dblk * DEV_BSIZE));
			return;
		} else if ((n = read(devfd, inodebuf, ibufsize)) != ibufsize) {
			warnx("Can't read inodes: %s",
			     (n < ibufsize) ? "short read" : strerror(errno));
			return;
		}

		for (n = 0; n < sblock->fs_ipg; n++, inumber++) {
			if (inumber >= ROOTINO) {
				if (printonly)
					(void)printf("ino %d gen %x\n", inumber,
						     inodebuf[n].di_gen);
				else
					inodebuf[n].di_gen = arc4random();
			}
		}

		/* Write out modified inodes */
		if (!printonly) {
			/* XXX - don't use DEV_BSIZE, get from disklabel! */
			if (lseek(devfd, (off_t)(dblk * DEV_BSIZE), SEEK_SET) < 0) {
				warn("Can't seek to %qd",
				    (off_t)(dblk * DEV_BSIZE));
				return;
			} else if ((n = write(devfd, inodebuf, ibufsize)) !=
				 ibufsize) {
				errx(1, "Can't write inodes: %s",
				     (n != ibufsize) ? "short write" :
				     strerror(errno));
				return;
			}
		}
	}
	(void)close(devfd);
}

void
usage(ex)
	int ex;
{
	(void)fprintf(stderr, "Usage: %s [ -p ] special [special ...]\n",
		      __progname);
	exit(ex);
}
