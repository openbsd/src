/*	$OpenBSD: mknod.c,v 1.6 1999/04/18 19:40:41 millert Exp $	*/
/*	$NetBSD: mknod.c,v 1.8 1995/08/11 00:08:18 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mknod.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: mknod.c,v 1.6 1999/04/18 19:40:41 millert Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <err.h>

extern char *__progname;

int domknod __P((int, char **, mode_t));
int domkfifo __P((int, char **, mode_t));
void usage __P((int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, ismkfifo = 0;
	void *set = NULL;
	mode_t mode;

	setlocale (LC_ALL, "");

	if (strcmp(__progname, "mkfifo") == 0)
		ismkfifo = 1;

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
			usage(ismkfifo);
		}
	argc -= optind;
	argv += optind;

	if (argv[0] == NULL)
		usage(ismkfifo);
	if (!ismkfifo) {
		if (argc == 2 && argv[1][0] == 'p') {
			ismkfifo = 2;
			argc--;
			argv[1] = NULL;
		} else if (argc != 4) {
			usage(ismkfifo);
			/* NOTREACHED */
		}
	}

	/* The default mode is the value of the bitwise inclusive or of
	   S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and S_IWOTH */
	if (!set)
		mode = 0666;

	if (ismkfifo)
		exit(domkfifo(argc, argv, mode));
	else
		exit(domknod(argc, argv, mode));
}

int
domknod(argc, argv, mode)
	int argc;
	char **argv;
	mode_t mode;
{
	dev_t dev;
	char *endp;
	u_int major, minor;

	if (argv[1][0] == 'c')
		mode |= S_IFCHR;
	else if (argv[1][0] == 'b')
		mode |= S_IFBLK;
	else {
		errx(1, "node must be type 'b' or 'c'.");
		/* NOTREACHED */
	}

	major = (long)strtoul(argv[2], &endp, 10);
	if (endp == argv[2] || *endp != '\0') {
		errx(1, "non-numeric major number.");
		/* NOTREACHED */
	}
	minor = (long)strtoul(argv[3], &endp, 10);
	if (endp == argv[3] || *endp != '\0') {
		errx(1, "non-numeric minor number.");
		/* NOTREACHED */
	}
	dev = makedev(major, minor);
	if (major(dev) != major || minor(dev) != minor) {
		errx(1, "major or minor number too large");
		/* NOTREACHED */
	}
	if (mknod(argv[0], mode, dev) < 0) {
		err(1, "%s", argv[0]);
		/* NOTREACHED */
	}
	return(0);
}

int
domkfifo(argc, argv, mode)
	int argc;
	char **argv;
	mode_t mode;
{
	int rv;

	for (rv = 0; *argv; ++argv) {
		if (mkfifo(*argv, mode) < 0) {  
			warn("%s", *argv);
			rv = 1;
		}
	}
	return(rv);
}

void
usage(ismkfifo)
	int ismkfifo;
{

	if (ismkfifo == 1)
		(void)fprintf(stderr, "usage: %s [-m mode] fifoname ...\n",
		    __progname);
	else {
		(void)fprintf(stderr, "usage: %s [-m mode] name [b | c] major minor\n",
		    __progname);
		(void)fprintf(stderr, "usage: %s [-m mode] name p\n",
		    __progname);
	}
	exit(1);
}
