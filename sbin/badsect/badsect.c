/*	$OpenBSD: badsect.c,v 1.17 2007/06/29 03:37:09 deraadt Exp $	*/
/*	$NetBSD: badsect.c,v 1.10 1995/03/18 14:54:28 cgd Exp $	*/

/*
 * Copyright (c) 1981, 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1981, 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)badsect.c	8.1 (Berkeley) 6/5/93";
#else
static const char rcsid[] = "$OpenBSD: badsect.c,v 1.17 2007/06/29 03:37:09 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * badsect
 *
 * Badsect takes a list of file-system relative sector numbers
 * and makes files containing the blocks of which these sectors are a part.
 * It can be used to contain sectors which have problems if these sectors
 * are not part of the bad file for the pack (see bad144).  For instance,
 * this program can be used if the driver for the file system in question
 * does not support bad block forwarding.
 */
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/stat.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>

#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

static int chkuse(daddr64_t, int);
static void rdfs(daddr64_t, int, char *);

static union {
	struct	fs fs;
	char	fsx[SBSIZE];
} ufs;
#define sblock	ufs.fs
static union {
	struct	cg cg;
	char	cgx[MAXBSIZE];
} ucg;
#define	acg	ucg.cg
static struct	fs *fs;
static int	fsi;
static int	errs;
static long	dev_bsize = 1;

int
main(int argc, char *argv[])
{
	daddr64_t number;
	struct stat stbuf, devstat;
	struct direct *dp;
	DIR *dirp;
	char name[BUFSIZ];
	int len;

	if (argc < 3) {
		fprintf(stderr, "usage: badsect bbdir blkno [ blkno ]\n");
		exit(1);
	}
	if (chdir(argv[1]) < 0 || stat(".", &stbuf) < 0)
		err(2, "%s", argv[1]);

	strlcpy(name, _PATH_DEV, sizeof name);
	len = strlen(name);
	if ((dirp = opendir(name)) == NULL)
		err(3, "%s", name);

	while ((dp = readdir(dirp)) != NULL) {
		strlcpy(&name[len], dp->d_name, sizeof name - len);
		if (stat(name, &devstat) < 0)
			err(4, "%s", name);

		if (stbuf.st_dev == devstat.st_rdev &&
		    S_ISBLK(devstat.st_mode))
			break;
	}

	/*
	 * We've found the block device, but since the filesystem
	 * is mounted, we must write to the raw (character) device
	 * instead. This is not guaranteed to work if someone has a
	 * /dev that doesn't follow standard naming conventions, but
	 * it's all we've got.
	 */
	name[len] = 'r';
	strlcpy(&name[len+1], dp->d_name, sizeof name - (len+1));
	closedir(dirp);
	if (dp == NULL)
		err(5, "Cannot find dev 0%o corresponding to %s",
			stbuf.st_rdev, argv[1]);

	if ((fsi = open(name, 0)) < 0)
		err(6, "%s", name);

	fs = &sblock;
	rdfs(SBOFF, SBSIZE, (char *)fs);
	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
	for (argc -= 2, argv += 2; argc > 0; argc--, argv++) {
		number = strtonum(*argv, 0, QUAD_MAX, NULL);
		if (chkuse(number, 1))
			continue;
		if (mknod(*argv, S_IFMT|S_IRUSR|S_IWUSR,
		    dbtofsb(fs, number)) < 0) {
			warn("%s", *argv);
			errs++;
		}
	}
	printf("Don't forget to run ``fsck %s''\n", name);
	exit(errs);
}

static int
chkuse(daddr64_t blkno, int cnt)
{
	int cg;
	daddr64_t fsbn, bn;

	fsbn = dbtofsb(fs, blkno);
	if (fsbn+cnt > fs->fs_ffs1_size) {
		fprintf(stderr, "block %lld out of range of file system\n", blkno);
		return (1);
	}
	cg = dtog(fs, fsbn);
	if (fsbn < cgdmin(fs, cg)) {
		if (cg == 0 || (fsbn+cnt) > cgsblock(fs, cg)) {
			fprintf(stderr, "block %lld in non-data area: cannot attach\n",
				blkno);
			return (1);
		}
	} else {
		if ((fsbn+cnt) > cgbase(fs, cg+1)) {
			fprintf(stderr, "block %lld in non-data area: cannot attach\n",
				blkno);
			return (1);
		}
	}
	rdfs(fsbtodb(fs, cgtod(fs, cg)), (int)sblock.fs_cgsize,
	    (char *)&acg);
	if (!cg_chkmagic(&acg)) {
		fprintf(stderr, "cg %d: bad magic number\n", cg);
		errs++;
		return (1);
	}
	bn = dtogd(fs, fsbn);
	if (isclr(cg_blksfree(&acg), bn))
		fprintf(stderr, "Warning: sector %lld is in use\n", blkno);
	return (0);
}

/*
 * read a block from the file system
 */
static void
rdfs(daddr64_t bno, int size, char *bf)
{
	int n;

	if (lseek(fsi, (off_t)bno * dev_bsize, SEEK_SET) < 0) {
		fprintf(stderr, "seek error: %lld\n", (long long)bno);
		err(1, "rdfs");
	}
	n = read(fsi, bf, size);
	if (n != size) {
		fprintf(stderr, "read error: %lld\n", (long long)bno);
		err(1, "rdfs");
	}
}
