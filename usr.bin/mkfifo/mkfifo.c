/*	$OpenBSD: mkfifo.c,v 1.5 1998/09/26 09:04:43 deraadt Exp $	*/
/*	$NetBSD: mkfifo.c,v 1.7 1994/12/23 07:16:56 jtc Exp $	*/

/*
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
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkfifo.c	8.2 (Berkeley) 1/5/94";
#endif
static char rcsid[] = "$OpenBSD: mkfifo.c,v 1.5 1998/09/26 09:04:43 deraadt Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <err.h>

static void usage();

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, exitval;
	void * set;
	mode_t mode;

	setlocale (LC_ALL, "");

	/* The default mode is the value of the bitwise inclusive or of
	   S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and S_IWOTH
	   modified by the file creation mask */
	mode = 0666 & ~umask(0);

	while ((ch = getopt(argc, argv, "m:")) != -1)
		switch(ch) {
		case 'm':
			if (!(set = setmode(optarg))) {
				errx(1, "invalid file mode.");
				/* NOTREACHED */
			}

			/*
			 * In symbolic mode strings, the + and - operators are
			 * interpreted relative to an assumed initial mode of
			 * a=rw.
			 */
			mode = getmode (set, 0666);
			free(set);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argv[0] == NULL)
		usage();

	for (exitval = 0; *argv; ++argv) {
		if (mkfifo(*argv, mode) < 0) {
			warn("%s", *argv);
			exitval = 1;
		}
	}
	exit(exitval);
}

void
usage()
{
	(void)fprintf(stderr, "usage: mkfifo [-m mode] fifoname ...\n");
	exit(1);
}
