/*	$OpenBSD: tunefs.c,v 1.16 2001/12/04 07:23:58 mickey Exp $	*/
/*	$NetBSD: tunefs.c,v 1.10 1995/03/18 15:01:31 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tunefs.c	8.2 (Berkeley) 4/19/94";
#else
static char rcsid[] = "$OpenBSD: tunefs.c,v 1.16 2001/12/04 07:23:58 mickey Exp $";
#endif
#endif /* not lint */

/*
 * tunefs: change layout parameters to an existing file system.
 */
#include <sys/param.h>
#include <sys/stat.h>

#include <ufs/ffs/fs.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <stdio.h>
#include <paths.h>
#include <stdlib.h>
#include <unistd.h>

/* the optimization warning string template */
#define	OPTWARN	"should optimize for %s with minfree %s %d%%"

union {
	struct	fs sb;
	char pad[MAXBSIZE];
} sbun;
#define	sblock sbun.sb

int fi = -1;
long dev_bsize = 1;

void bwrite(daddr_t, char *, int);
int bread(daddr_t, char *, int);
void getsb(struct fs *, char *, int);
void usage __P((void));
void printfs __P((void));

extern char *__progname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp, *special, *name;
	struct stat st;
	int i;
	int Aflag = 0;
	struct fstab *fs;
	char *chg[2], device[MAXPATHLEN];

	argc--, argv++; 
	if (argc < 2)
		usage();
	special = argv[argc - 1];
	fs = getfsfile(special);
	if (fs)
		special = fs->fs_spec;
again:
	if (stat(special, &st) < 0) {
		if (*special != '/') {
			if (*special == 'r')
				special++;
			(void)snprintf(device, sizeof(device), "%s/%s",
				       _PATH_DEV, special);
			special = device;
			goto again;
		}
		err(1, "%s", special);
	}
	if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode))
		errx(10, "%s: not a block or character device", special);
	for (; argc > 0 && argv[0][0] == '-'; argc--, argv++) {
		for (cp = &argv[0][1]; *cp; cp++)
			switch (*cp) {

			case 'A':
				Aflag++;
				continue;

			case 'p':
				getsb(&sblock, special, O_RDONLY);
				printfs();
				return (0);

			case 'a':
				getsb(&sblock, special, O_RDWR);
				name = "maximum contiguous block count";
				if (argc < 1)
					errx(10, "-a: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_maxcontig, i);
				sblock.fs_maxcontig = i;
				continue;

			case 'd':
				getsb(&sblock, special, O_RDWR);
				name =
				   "rotational delay between contiguous blocks";
				if (argc < 1)
					errx(10, "-d: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				warnx("%s changes from %dms to %dms",
				    name, sblock.fs_rotdelay, i);
				sblock.fs_rotdelay = i;
				continue;

			case 'e':
				getsb(&sblock, special, O_RDWR);
				name =
				  "maximum blocks per file in a cylinder group";
				if (argc < 1)
					errx(10, "-e: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_maxbpg, i);
				sblock.fs_maxbpg = i;
				continue;

			case 'f':
				getsb(&sblock, special, O_RDWR);
				name = "average file size";
				if (argc < 1)
					errx(10, "-f: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 0)
					errx(10, "%s must be >= 0 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_avgfilesize, i);
				sblock.fs_avgfilesize = i;
				continue;

			case 'm':
				getsb(&sblock, special, O_RDWR);
				name = "minimum percentage of free space";
				if (argc < 1)
					errx(10, "-m: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 0 || i > 99)
					errx(10, "bad %s (%s)", name, *argv);
				warnx("%s changes from %d%% to %d%%",
				    name, sblock.fs_minfree, i);
				sblock.fs_minfree = i;
				if (i >= MINFREE &&
				    sblock.fs_optim == FS_OPTSPACE)
					warnx(OPTWARN, "time", ">=", MINFREE);
				if (i < MINFREE &&
				    sblock.fs_optim == FS_OPTTIME)
					warnx(OPTWARN, "space", "<", MINFREE);
				continue;

			case 'n':
				getsb(&sblock, special, O_RDWR);
				name = "expected number of files per directory";
				if (argc < 1)
					errx(10, "-n: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 0)
					errx(10, "%s must be >= 0 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_avgfpdir, i);
				sblock.fs_avgfpdir = i;
				continue;

			case 's':
				errx(1, "See mount(8) for details about"
				      " how to enable soft updates.");

			case 'o':
				getsb(&sblock, special, O_RDWR);
				name = "optimization preference";
				if (argc < 1)
					errx(10, "-o: missing %s", name);
				argc--, argv++;
				chg[FS_OPTSPACE] = "space";
				chg[FS_OPTTIME] = "time";
				if (strcmp(*argv, chg[FS_OPTSPACE]) == 0)
					i = FS_OPTSPACE;
				else if (strcmp(*argv, chg[FS_OPTTIME]) == 0)
					i = FS_OPTTIME;
				else
					errx(10, "bad %s (options are `space' or `time')",
					    name);
				if (sblock.fs_optim == i) {
					warnx("%s remains unchanged as %s",
					    name, chg[i]);
					continue;
				}
				warnx("%s changes from %s to %s",
				    name, chg[sblock.fs_optim], chg[i]);
				sblock.fs_optim = i;
				if (sblock.fs_minfree >= MINFREE &&
				    i == FS_OPTSPACE)
					warnx(OPTWARN, "time", ">=", MINFREE);
				if (sblock.fs_minfree < MINFREE &&
				    i == FS_OPTTIME)
					warnx(OPTWARN, "space", "<", MINFREE);
				continue;

			default:
				usage();
			}
	}
	if (argc != 1)
		usage();
	bwrite((daddr_t)SBOFF / dev_bsize, (char *)&sblock, SBSIZE);
	if (Aflag)
		for (i = 0; i < sblock.fs_ncg; i++)
			bwrite(fsbtodb(&sblock, cgsblock(&sblock, i)),
			    (char *)&sblock, SBSIZE);
	if (close(fi))
		err(1, "close: %s", special);

	return (0);
}

void
usage()
{
	fprintf(stderr,
		"Usage: %s tuneup-options special-device\n"
		"where tuneup-options are:\n"
		"\t-A modify all backups of the super-block\n"
		"\t-a maximum contiguous blocks\n"
		"\t-d rotational delay between contiguous blocks\n"
		"\t-e maximum blocks per file in a cylinder group\n"
		"\t-f expected average file size\n"
		"\t-m minimum percentage of free space\n"
		"\t-n expected number of files per directory\n"
		"\t-o optimization preference (`space' or `time')\n"
		"\t-p no change - just prints current tuneable settings\n",
		__progname);
	exit(2);
}

void
getsb(fs, file, flags)
	struct fs *fs;
	char *file;
	int flags;
{

	if (fi >= 0)
		return;
	fi = open(file, flags);
	if (fi < 0)
		err(3, "cannot open %s", file);
	if (bread((daddr_t)SBOFF, (char *)fs, SBSIZE))
		err(4, "%s: bad super block", file);
	if (fs->fs_magic != FS_MAGIC)
		errx(5, "%s: bad magic number", file);
	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
}

void
printfs()
{
	warnx("maximum contiguous block count: (-a)               %d",
	      sblock.fs_maxcontig);
	warnx("rotational delay between contiguous blocks: (-d)   %d ms",
	      sblock.fs_rotdelay);
	warnx("maximum blocks per file in a cylinder group: (-e)  %d",
	      sblock.fs_maxbpg);
	warnx("expected average file size: (-f)                   %d",
	      sblock.fs_avgfilesize);
	warnx("minimum percentage of free space: (-m)             %d%%",
	      sblock.fs_minfree);
	warnx("expected number of files per directory: (-n)       %d",
	      sblock.fs_avgfpdir);
	warnx("optimization preference: (-o)                      %s",
	      sblock.fs_optim == FS_OPTSPACE ? "space" : "time");
	if (sblock.fs_minfree >= MINFREE &&
	    sblock.fs_optim == FS_OPTSPACE)
		warnx(OPTWARN, "time", ">=", MINFREE);
	if (sblock.fs_minfree < MINFREE &&
	    sblock.fs_optim == FS_OPTTIME)
		warnx(OPTWARN, "space", "<", MINFREE);
}

void
bwrite(blk, buf, size)
	daddr_t blk;
	char *buf;
	int size;
{

	if (lseek(fi, (off_t)blk * dev_bsize, SEEK_SET) < 0)
		err(6, "FS SEEK");
	if (write(fi, buf, size) != size)
		err(7, "FS WRITE");
}

int
bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
	int cnt;
{
	int i;

	if (lseek(fi, (off_t)bno * dev_bsize, SEEK_SET) < 0)
		return(1);
	if ((i = read(fi, buf, cnt)) != cnt) {
		for(i=0; i<sblock.fs_bsize; i++)
			buf[i] = 0;
		return (1);
	}
	return (0);
}
