/*	$OpenBSD: nice.c,v 1.17 2016/10/28 07:22:59 schwarze Exp $	*/
/*	$NetBSD: nice.c,v 1.9 1995/08/31 23:30:58 jtc Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
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

#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	DEFNICE	10

static void __dead usage(void);

int
main(int argc, char *argv[])
{
	const char *errstr;
	int prio = DEFNICE;
	int c;

	if (pledge("stdio exec proc", NULL) == -1)
		err(1, "pledge");

	/* handle obsolete -number syntax */
	if (argc > 1 && argv[1][0] == '-' &&
	    isdigit((unsigned char)argv[1][1])) {
		prio = strtonum(argv[1] + 1, PRIO_MIN, PRIO_MAX, &errstr);
		if (errstr)
			errx(1, "increment is %s", errstr);
		argc--;
		argv++;
	}

	while ((c = getopt (argc, argv, "n:")) != -1) {
		switch (c) {
		case 'n':
			prio = strtonum(optarg, PRIO_MIN, PRIO_MAX, &errstr);
			if (errstr)
				errx(1, "increment is %s", errstr);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	errno = 0;
	prio += getpriority(PRIO_PROCESS, 0);
	if (errno)
		err(1, "getpriority");
	if (setpriority(PRIO_PROCESS, 0, prio))
		warn("setpriority");

	if (pledge("stdio exec", NULL) == -1)
		err(1, "pledge");

	execvp(argv[0], &argv[0]);
	err((errno == ENOENT) ? 127 : 126, "%s", argv[0]);
}

static void __dead
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-n increment] utility [argument ...]\n",
	    __progname);
	exit(1);
}
