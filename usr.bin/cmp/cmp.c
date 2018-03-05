/*      $OpenBSD: cmp.c,v 1.18 2018/03/05 16:57:37 cheloha Exp $      */
/*      $NetBSD: cmp.c,v 1.7 1995/09/08 03:22:56 tls Exp $      */

/*
 * Copyright (c) 1987, 1990, 1993, 1994
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

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

int	lflag, sflag;

static off_t get_skip(const char *, const char *);
static void __dead usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb1, sb2;
	off_t skip1, skip2;
	int ch, fd1, fd2, special;
	char *file1, *file2;

	if (pledge("stdio rpath", NULL) == -1)
		err(ERR_EXIT, "pledge");

	while ((ch = getopt(argc, argv, "ls")) != -1)
		switch (ch) {
		case 'l':		/* print all differences */
			lflag = 1;
			break;
		case 's':		/* silent run */
			sflag = 1;
			break;
		default:
			usage();
		}

	argv += optind;
	argc -= optind;

	if (lflag && sflag)
		errx(ERR_EXIT, "only one of -l and -s may be specified");

	if (argc < 2 || argc > 4)
		usage();

	/* Backward compatibility -- handle "-" meaning stdin. */
	special = 0;
	if (strcmp(file1 = argv[0], "-") == 0) {
		special = 1;
		fd1 = 0;
		file1 = "stdin";
	} else if ((fd1 = open(file1, O_RDONLY, 0)) == -1)
		fatal("%s", file1);
	if (strcmp(file2 = argv[1], "-") == 0) {
		if (special)
			fatalx("standard input may only be specified once");
		special = 1;
		fd2 = 0;
		file2 = "stdin";
	} else if ((fd2 = open(file2, O_RDONLY, 0)) == -1)
		fatal("%s", file2);

	if (pledge("stdio", NULL) == -1)
		err(ERR_EXIT, "pledge");

	skip1 = (argc > 2) ? get_skip(argv[2], "skip1") : 0;
	skip2 = (argc == 4) ? get_skip(argv[3], "skip2") : 0;

	if (!special) {
		if (fstat(fd1, &sb1) == -1)
			fatal("%s", file1);
		if (!S_ISREG(sb1.st_mode))
			special = 1;
		else {
			if (fstat(fd2, &sb2) == -1)
				fatal("%s", file2);
			if (!S_ISREG(sb2.st_mode))
				special = 1;
		}
	}

	if (special)
		c_special(fd1, file1, skip1, fd2, file2, skip2);
	else
		c_regular(fd1, file1, skip1, sb1.st_size,
		    fd2, file2, skip2, sb2.st_size);
	return 0;
}

static off_t
get_skip(const char *arg, const char *name)
{
	off_t skip;
	char *ep;

	errno = 0;
	skip = strtoll(arg, &ep, 0);
	if (arg[0] == '\0' || *ep != '\0')
		fatalx("%s is invalid: %s", name, arg);
	if (skip < 0)
		fatalx("%s is too small: %s", name, arg);
	if (skip == LLONG_MAX && errno == ERANGE)
		fatalx("%s is too large: %s", name, arg);
	return skip;
}

static void __dead
usage(void)
{

	(void)fprintf(stderr,
	    "usage: cmp [-l | -s] file1 file2 [skip1 [skip2]]\n");
	exit(ERR_EXIT);
}
