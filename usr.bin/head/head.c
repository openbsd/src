/*	$OpenBSD: head.c,v 1.4 1999/07/23 13:14:32 aaron Exp $	*/

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
char copyright[] =
"@(#) Copyright (c) 1980, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)head.c	5.5 (Berkeley) 6/1/90";*/
static char rcsid[] = "$OpenBSD: head.c,v 1.4 1999/07/23 13:14:32 aaron Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

static void usage ();

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	register long cnt;
	int	ch, firsttime;
	long	linecnt = 10;
	char	*inval = NULL, *p = NULL;
	size_t	len;

	/* handle obsolete -number syntax */
	if (argc > 1 && argv[1][0] == '-' && isdigit(argv[1][1])) {
		linecnt = strtol((p = argv[1] + 1), &inval, 10);
		argc--; argv++;
	}

	while ((ch = getopt (argc, argv, "n:")) != -1)
		switch (ch) {
		case 'n':
			linecnt = strtol((p = optarg), &inval, 10);
			break;
		default:
			usage();	
		}
	argc -= optind, argv += optind;

	if (p) {
		if ((linecnt == LONG_MIN || linecnt == LONG_MAX) &&
		    errno == ERANGE)
			err(1, "invalid line count: %s", p);
		else if (linecnt <= 0 || *inval)
			errx(1, "invalid line count: %s", p);
	}

	/* setlinebuf(stdout); */
	for (firsttime = 1; ; firsttime = 0) {
		if (!*argv) {
			if (!firsttime)
				exit(0);
		}
		else {
			if (!freopen(*argv, "r", stdin)) {
				fprintf(stderr, "head: can't read %s.\n", *argv++);
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
usage ()
{
	fputs("usage: head [-n line_count] [file ...]\n", stderr);
	exit(1);
}

