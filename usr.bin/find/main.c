/*	$OpenBSD: main.c,v 1.26 2009/12/20 16:15:26 schwarze Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
#include <fts.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "find.h"

time_t now;			/* time find was run */
int dotfd;			/* starting directory; may be -1 */
int ftsoptions;			/* options for the fts_open(3) call */
int isdepth;			/* do directories on post-order visit */
int isoutput;			/* user specified output operator */
int isxargs;			/* don't permit xargs delimiting chars */

__dead static void usage(void);

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	char **p, **paths, **paths2;
	int ch;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = show_path;
	sa.sa_flags = SA_RESTART; 

	(void)time(&now);	/* initialize the time-of-day */

	p = paths = (char **) emalloc(sizeof(char *) * argc);

	sigaction(SIGINFO, &sa, NULL);

	ftsoptions = FTS_NOSTAT|FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "Hdf:hLXx")) != -1)
		switch(ch) {
		case 'H':
			ftsoptions |= FTS_COMFOLLOW;
			ftsoptions |= FTS_PHYSICAL;
			ftsoptions &= ~FTS_LOGICAL;
			break;
		case 'd':
			isdepth = 1;
			break;
		case 'f':
			*p++ = optarg;
			break;
		case 'h':
		case 'L':
			ftsoptions &= ~FTS_COMFOLLOW;
			ftsoptions &= ~FTS_PHYSICAL;
			ftsoptions |= FTS_LOGICAL;
			break;
		case 'X':
			isxargs = 1;
			break;
		case 'x':
			ftsoptions &= ~FTS_NOSTAT;
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			break;
		}

	argc -= optind;	
	argv += optind;

	/* The first argument that starts with a -, or is a ! or a (, and all
	 * subsequent arguments shall be interpreted as an expression ...
	 * (POSIX.2).
	 */
	while (*argv) {
		if (**argv == '-' ||
		    ((**argv == '!' || **argv == '(') && (*argv)[1] == '\0'))
			break;
		*p++ = *argv++;
	}

	if (p == paths)
		usage();
	*p = NULL;

	if (!(paths2 = realloc(paths, sizeof(char *) * (p - paths + 1))))
		err(1, NULL);
	paths = paths2;

	dotfd = open(".", O_RDONLY, 0);

	find_execute(find_formplan(argv), paths);
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: find [-dHhLXx] [-f path] path ... [expression]\n");
	exit(1);
}
