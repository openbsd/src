/*	$OpenBSD: biff.c,v 1.16 2018/08/11 10:59:34 mestre Exp $	*/
/*	$NetBSD: biff.c,v 1.3 1995/03/26 02:34:22 glass Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb;
	int ch;
	char *name;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((name = ttyname(STDERR_FILENO)) == NULL)
		err(2, "tty");

	if (unveil(name, "rw") == -1)
		err(2, "unveil");
	if (pledge("stdio rpath fattr", NULL) == -1)
		err(2, "pledge");

	if (stat(name, &sb))
		err(2, "stat");

	sb.st_mode &= ACCESSPERMS;

	if (*argv == NULL) {
		(void)printf("is %s\n", sb.st_mode & S_IXUSR ? "y" : "n");
		exit(sb.st_mode & S_IXUSR ? 0 : 1);
	}

	switch(argv[0][0]) {
	case 'n':
		if (chmod(name, sb.st_mode & ~S_IXUSR) < 0)
			err(2, "%s", name);
		break;
	case 'y':
		if (chmod(name, sb.st_mode | S_IXUSR) < 0)
			err(2, "%s", name);
		break;
	default:
		usage();
	}
	exit(sb.st_mode & S_IXUSR ? 0 : 1);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: biff [n | y]\n");
	exit(2);
}
