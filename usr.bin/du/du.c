/*	$OpenBSD: du.c,v 1.15 2004/06/02 14:58:46 tom Exp $	*/
/*	$NetBSD: du.c,v 1.11 1996/10/18 07:20:35 thorpej Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)du.c	8.5 (Berkeley) 5/4/95";
#else
static char rcsid[] = "$OpenBSD: du.c,v 1.15 2004/06/02 14:58:46 tom Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum { NONE = 0, KILO, MEGA, GIGA, TERA, PETA /* , EXA */ } unit_t;

int	 linkchk(FTSENT *);
void	 prtout(quad_t, char *, int);
void	 usage(void);
unit_t	 unit_adjust(double *);

int
main(int argc, char *argv[])
{
	FTS *fts;
	FTSENT *p;
	long blocksize;
	quad_t totalblocks;
	int ftsoptions, listdirs, listfiles;
	int Hflag, Lflag, Pflag, aflag, cflag, hflag, kflag, sflag;
	int ch, notused, rval;
	char **save;

	save = argv;
	Hflag = Lflag = Pflag = aflag = cflag = hflag = kflag = sflag = 0;
	totalblocks = 0;
	ftsoptions = FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "HLPachksxr")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'k':
			kflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'r':
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Because of the way that fts(3) works, logical walks will not count
	 * the blocks actually used by symbolic links.  We rationalize this by
	 * noting that users computing logical sizes are likely to do logical
	 * copies, so not counting the links is correct.  The real reason is
	 * that we'd have to re-implement the kernel's symbolic link traversing
	 * algorithm to get this right.  If, for example, you have relative
	 * symbolic links referencing other relative symbolic links, it gets
	 * very nasty, very fast.  The bottom line is that it's documented in
	 * the man page, so it's a feature.
	 */
	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;
	if (Lflag) {
		ftsoptions &= ~FTS_PHYSICAL;
		ftsoptions |= FTS_LOGICAL;
	}

	if (aflag) {
		if (sflag)
			usage();
		listdirs = listfiles = 1;
	} else if (sflag)
		listdirs = listfiles = 0;
	else {
		listfiles = 0;
		listdirs = 1;
	}

	if (!*argv) {
		argv = save;
		argv[0] = ".";
		argv[1] = NULL;
	}

	if (hflag)
		blocksize = 512;
	else if (kflag)
		blocksize = 1024;
	else
		(void)getbsize(&notused, &blocksize);
	blocksize /= 512;

	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	for (rval = 0; (p = fts_read(fts)) != NULL;)
		switch (p->fts_info) {
		case FTS_D:			/* Ignore. */
			break;
		case FTS_DP:
			p->fts_parent->fts_number += 
			    p->fts_number += p->fts_statp->st_blocks;
			if (cflag)
				totalblocks += p->fts_statp->st_blocks;
			/*
			 * If listing each directory, or not listing files
			 * or directories and this is post-order of the
			 * root of a traversal, display the total.
			 */
			if (listdirs || (!listfiles && !p->fts_level))
				prtout((quad_t)howmany(p->fts_number, blocksize),
				    p->fts_path, hflag);
			break;
		case FTS_DC:			/* Ignore. */
			break;
		case FTS_DNR:			/* Warn, continue. */
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		default:
			if (p->fts_statp->st_nlink > 1 && linkchk(p))
				break;
			/*
			 * If listing each file, or a non-directory file was
			 * the root of a traversal, display the total.
			 */
			if (listfiles || !p->fts_level)
				prtout(howmany(p->fts_statp->st_blocks, blocksize),
				    p->fts_path, hflag);
			p->fts_parent->fts_number += p->fts_statp->st_blocks;
			if (cflag)
				totalblocks += p->fts_statp->st_blocks;
		}
	if (errno)
		err(1, "fts_read");
	if (cflag) {
		prtout((quad_t)howmany(totalblocks, blocksize), "total", hflag);
	}
	exit(rval);
}

typedef struct _ID {
	dev_t	dev;
	ino_t	inode;
} ID;

int
linkchk(FTSENT *p)
{
	static ID *files;
	static int maxfiles, nfiles;
	ID *fp, *start;
	ino_t ino;
	dev_t dev;

	ino = p->fts_statp->st_ino;
	dev = p->fts_statp->st_dev;
	if ((start = files) != NULL)
		for (fp = start + nfiles - 1; fp >= start; --fp)
			if (ino == fp->inode && dev == fp->dev)
				return (1);

	if (nfiles == maxfiles && (files = realloc((char *)files,
	    (u_int)(sizeof(ID) * (maxfiles += 128)))) == NULL)
		err(1, NULL);
	files[nfiles].inode = ino;
	files[nfiles].dev = dev;
	++nfiles;
	return (0);
}

/*
 * "human-readable" output: use 3 digits max.--put unit suffixes at
 * the end.  Makes output compact and easy-to-read. 
 */

unit_t
unit_adjust(double *val)
{
	double abval;
	unit_t unit;

	abval = fabs(*val);
	if (abval < 1024)
		unit = NONE;
	else if (abval < 1048576ULL) {
		unit = KILO;
		*val /= 1024;
	} else if (abval < 1073741824ULL) {
		unit = MEGA;
		*val /= 1048576;
	} else if (abval < 1099511627776ULL) {
		unit = GIGA;
		*val /= 1073741824ULL;
	} else if (abval < 1125899906842624ULL) {
		unit = TERA;
		*val /= 1099511627776ULL;
	} else /* if (abval < 1152921504606846976ULL) */ {
		unit = PETA;
		*val /= 1125899906842624ULL;
	}
	return (unit);
}

void
prtout(quad_t size, char *path, int hflag)
{
	unit_t unit;
	double bytes;

	if (!hflag)
		(void)printf("%lld\t%s\n", (long long)size, path);
	else {
		bytes = (double)size * 512.0;
		unit = unit_adjust(&bytes);

		if (bytes == 0)
			(void)printf("0B\t%s\n", path);
		else if (bytes > 10)
			(void)printf("%.0f%c\t%s\n", bytes, "BKMGTPE"[unit], path);
		else
			(void)printf("%.1f%c\t%s\n", bytes, "BKMGTPE"[unit], path);
	}
}

void
usage(void)
{

	(void)fprintf(stderr,
		"usage: du [-H | -L | -P] [-a | -s] [-chkrx] [file ...]\n");
	exit(1);
}
