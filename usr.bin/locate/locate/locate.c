/*
 *	$OpenBSD: locate.c,v 1.33 2019/01/17 06:15:44 tedu Exp $
 *
 * Copyright (c) 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
 * Copyright (c) 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods.
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

/*
 * Ref: Usenix ;login:, Vol 8, No 1, February/March, 1983, p. 8.
 *
 * Locate scans a file list for the full pathname of a file given only part
 * of the name.  The list has been processed with "front-compression"
 * and bigram coding.  Front compression reduces space by a factor of 4-5,
 * bigram coding by a further 20-25%.
 *
 * The codes are:
 *
 *      0-28    likeliest differential counts + offset to make nonnegative
 *      30      switch code for out-of-range count to follow in next word
 *      31      an 8 bit char followed
 *      128-255 bigram codes (128 most common, as determined by 'updatedb')
 *      32-127  single character (printable) ascii residue (ie, literal)
 *
 * A novel two-tiered string search technique is employed:
 *
 * First, a metacharacter-free subpattern and partial pathname is matched
 * BACKWARDS to avoid full expansion of the pathname list.  The time savings
 * is 40-50% over forward matching, which cannot efficiently handle
 * overlapped search patterns and compressed path residue.
 *
 * Then, the actual shell glob-style regular expression (if in this form) is
 * matched against the candidate pathnames using the slower routines provided
 * in the standard 'find'.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "locate.h"
#include "pathnames.h"

#ifdef DEBUG
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/resource.h>
#endif

char *path_fcodes;      /* locate database */
int f_mmap;             /* use mmap */
int f_icase;            /* ignore case */
int f_statistic;        /* print statistic */
int f_silent;           /* suppress output, show only count of matches */
int f_limit;            /* limit number of output lines, 0 == infinite */
int f_basename;		/* match only on the basename */
u_int counter;          /* counter for matches [-c] */


void    usage(void);
void    statistic(FILE *, char *);
void    fastfind(FILE *, char *, char *);
void    fastfind_icase(FILE *, char *, char *);
void    fastfind_mmap(char *, caddr_t, int, char *);
void    fastfind_mmap_icase(char *, caddr_t, int, char *);
void	search_mmap(char *, char **);
void	search_statistic(char *, char **);
unsigned long cputime(void);

extern char     **colon(char **, char*, char*);
extern int      getwm(caddr_t);
extern int      getwf(FILE *);
extern int	check_bigram_char(int);
extern char 	*patprep(char *);


int
main(int argc, char *argv[])
{
	int ch;
	char **dbv = NULL;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "bScd:il:")) != -1)
		switch (ch) {
		case 'b':
			f_basename = 1;
			break;
		case 'S':	/* statistic lines */
			f_statistic = 1;
			break;
		case 'l': /* limit number of output lines, 0 == infinite */
			f_limit = atoi(optarg);
			break;
		case 'd':	/* database */
			dbv = colon(dbv, optarg, _PATH_FCODES);
			break;
		case 'i':	/* ignore case */
			f_icase = 1;
			break;
		case 'c': /* suppress output, show only count of matches */
			f_silent = 1;
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	/* to few arguments */
	if (argc < 1 && !(f_statistic))
		usage();

	/* no (valid) database as argument */
	if (dbv == NULL || *dbv == NULL) {
		/* try to read database from environment */
		if ((path_fcodes = getenv("LOCATE_PATH")) == NULL ||
		    *path_fcodes == '\0')
			/* use default database */
			dbv = colon(dbv, _PATH_FCODES, _PATH_FCODES);
		else		/* $LOCATE_PATH */
			dbv = colon(dbv, path_fcodes, _PATH_FCODES);
	}

	/* foreach database ... */
	while ((path_fcodes = *dbv) != NULL) {
		dbv++;

		if (f_statistic)
			search_statistic(path_fcodes, argv);
		else
			search_mmap(path_fcodes, argv);
	}

	if (f_silent)
		printf("%u\n", counter);
	exit(0);
}


void
search_statistic(char *db, char **s)
{
	FILE *fp;
#ifdef DEBUG
	long t0;
#endif

	if ((fp = fopen(path_fcodes, "r")) == NULL)
		err(1,  "`%s'", path_fcodes);

	/* count only chars or lines */
	statistic(fp, path_fcodes);
	(void)fclose(fp);
}

void
search_mmap(char *db, char **s)
{
	struct stat sb;
	int fd;
	caddr_t p;
	off_t len;
#ifdef DEBUG
	long t0;
#endif
	if ((fd = open(path_fcodes, O_RDONLY)) == -1 ||
	    fstat(fd, &sb) == -1)
		err(1, "`%s'", path_fcodes);
	len = sb.st_size;
	if (len < (2*NBG))
		errx(1, "database too small: %s", db);

	if ((p = mmap((caddr_t)0, (size_t)len, PROT_READ, MAP_SHARED,
	    fd, (off_t)0)) == MAP_FAILED)
		err(1, "mmap ``%s''", path_fcodes);

	/* foreach search string ... */
	while (*s != NULL) {
#ifdef DEBUG
		t0 = cputime();
#endif
		if (f_icase)
			fastfind_mmap_icase(*s, p, (int)len, path_fcodes);
		else
			fastfind_mmap(*s, p, (int)len, path_fcodes);
#ifdef DEBUG
		(void)fprintf(stderr, "fastfind %ld ms\n", cputime () - t0);
#endif
		s++;
	}

	if (munmap(p, (size_t)len) == -1)
		warn("munmap %s", path_fcodes);

	(void)close(fd);
}

#ifdef DEBUG
unsigned long
cputime(void)
{
	struct rusage rus;

	getrusage(RUSAGE_SELF, &rus);
	return(rus.ru_utime.tv_sec * 1000 + rus.ru_utime.tv_usec / 1000);
}
#endif /* DEBUG */

void
usage(void)
{
	(void)fprintf(stderr, "usage: locate [-bciS] [-d database] ");
	(void)fprintf(stderr, "[-l limit] pattern ...\n");
	(void)fprintf(stderr, "default database: `%s' or $LOCATE_PATH\n",
	    _PATH_FCODES);
	exit(1);
}

void
sane_count(int count)
{
	if (count < 0 || count >= PATH_MAX) {
		fprintf(stderr, "locate: corrupted database\n");
		exit(1);
	}
}

/* load fastfind functions */

#undef FF_ICASE
#include "fastfind.c"
#define FF_ICASE
#include "fastfind.c"
