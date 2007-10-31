/*	$OpenBSD: head.c,v 1.14 2007/10/31 16:29:50 jmc Exp $	*/

/*
 * Copyright (c) 1980, 1987 Regents of the University of California.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
/*static char sccsid[] = "from: @(#)head.c	5.5 (Berkeley) 6/1/90";*/
static const char rcsid[] = "$OpenBSD: head.c,v 1.14 2007/10/31 16:29:50 jmc Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

static void usage(void);

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

int
main(int argc, char *argv[])
{
	long 	cnt;
	int	ch, firsttime;
	long	linecnt = 10;
	char	*p = NULL;

	/* handle obsolete -number syntax */
	if (argc > 1 && argv[1][0] == '-' && isdigit(argv[1][1])) {
		p = argv[1] + 1;
		argc--;
		argv++;
	}

	while ((ch = getopt(argc, argv, "n:")) != -1) {
		switch (ch) {
		case 'n':
			p = optarg;
			break;
		default:
			usage();	
		}
	}
	argc -= optind, argv += optind;

	if (p) {
		const char *errstr;

		linecnt = strtonum(p, 1, LONG_MAX, &errstr);
		if (errstr)
			errx(1, "line count %s: %s", errstr, p);
	}

	/* setlinebuf(stdout); */
	for (firsttime = 1; ; firsttime = 0) {
		if (!*argv) {
			if (!firsttime)
				exit(0);
		} else {
			if (!freopen(*argv, "r", stdin)) {
				warn("%s", *argv++);
				continue;
			}
			if (argc > 1) {
				if (!firsttime)
					putchar('\n');
				printf("==> %s <==\n", *argv);
			}
			++argv;
		}
		for (cnt = linecnt; cnt && !feof(stdin); --cnt)
			while ((ch = getchar()) != EOF)
				if (putchar(ch) == '\n')
					break;
	}
	/*NOTREACHED*/
}


static void
usage(void)
{
	fputs("usage: head [-count | -n count] [file ...]\n", stderr);
	exit(1);
}
