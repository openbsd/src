/*	$OpenBSD: wc.c,v 1.3 1999/02/02 03:44:07 millert Exp $	*/

/*
 * Copyright (c) 1980, 1987, 1991, 1993
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
"@(#) Copyright (c) 1980, 1987, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wc.c	8.2 (Berkeley) 5/2/95";
#else
static char rcsid[] = "$OpenBSD: wc.c,v 1.3 1999/02/02 03:44:07 millert Exp $";
#endif
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <err.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

int64_t	tlinect, twordct, tcharct;
int	doline, doword, dochar;
int 	rval;
extern char *__progname;

void	print_counts __P((int64_t, int64_t, int64_t, char *));
void	cnt __P((char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "lwcm")) != -1)
		switch((char)ch) {
		case 'l':
			doline = 1;
			break;
		case 'w':
			doword = 1;
			break;
		case 'c':
		case 'm':
			dochar = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: %s [-c | -m] [-lw] [file ...]\n",
			    __progname);
			exit(1);
		}
	argv += optind;
	argc -= optind;

	/*
	 * wc is unusual in that its flags are on by default, so,
	 * if you don't get any arguments, you have to turn them
	 * all on.
	 */
	if (!doline && !doword && !dochar)
		doline = doword = dochar = 1;

	if (!*argv) {
		cnt((char *)NULL);
	} else {
		int dototal = (argc > 1);

		do {
			cnt(*argv);
		} while(*++argv);

		if (dototal)
			print_counts(tlinect, twordct, tcharct, "total"); 
	}

	exit(rval);
}

void
cnt(file)
	char *file;
{
	register u_char *C;
	register short gotsp;
	register int len;
	register int64_t linect, wordct, charct;
	struct stat sbuf;
	int fd;
	u_char buf[MAXBSIZE];

	linect = wordct = charct = 0;
	if (file) {
		if ((fd = open(file, O_RDONLY, 0)) < 0) {
			warn("%s", file);
			rval = 1;
			return;
		}
	} else  {
		fd = STDIN_FILENO;
	}
	
	if (!doword) {
		/*
		 * Line counting is split out because it's a lot
		 * faster to get lines than to get words, since
		 * the word count requires some logic.
		 */
		if (doline) {
			while ((len = read(fd, buf, MAXBSIZE)) > 0) {
				charct += len;
				for (C = buf; len--; ++C)
					if (*C == '\n')
						++linect;
			}
			if (len == -1) {
				warn("%s", file);
				rval = 1;
			}
		}
		/*
		 * If all we need is the number of characters and
		 * it's a directory or a regular or linked file, just
		 * stat the puppy.  We avoid testing for it not being
		 * a special device in case someone adds a new type
		 * of inode.
		 */
		else if (dochar) {
			mode_t ifmt;

			if (fstat(fd, &sbuf)) {
				warn("%s", file);
				rval = 1;
			} else {
				ifmt = sbuf.st_mode & S_IFMT;
				if (ifmt == S_IFREG || ifmt == S_IFLNK
				    || ifmt == S_IFDIR) {
					charct = sbuf.st_size;
				} else {
					while ((len = read(fd, buf, MAXBSIZE)) > 0)
						charct += len;
					if (len == -1) {
						warn("%s", file);
						rval = 1;
					}
				}
			}
		}
	} else {
		/* Do it the hard way... */
		gotsp = 1;
		while ((len = read(fd, buf, MAXBSIZE)) > 0) {
			/*
			 * This loses in the presence of multi-byte characters.
			 * To do it right would require a function to return a
			 * character while knowing how many bytes it consumed.
			 */
			charct += len;
			for (C = buf; len--; ++C) {
				if (isspace (*C)) {
					gotsp = 1;
					if (*C == '\n')
						++linect;
				} else {
					/*
					 * This line implements the POSIX
					 * spec, i.e. a word is a "maximal
					 * string of characters delimited by
					 * whitespace."  Notice nothing was
					 * said about a character being
					 * printing or non-printing.
					 */
					if (gotsp) {
						gotsp = 0;
						++wordct;
					}
				}
			}
		}
		if (len == -1) {
			warn("%s", file);
			rval = 1;
		}
	}

	print_counts(linect, wordct, charct, file ? file : "");

	/*
	 * Don't bother checking doline, doword, or dochar -- speeds
         * up the common case
	 */
	tlinect += linect;
	twordct += wordct;
	tcharct += charct;

	if (close(fd) != 0) {
		warn("%s", file);
		rval = 1;
	}
}

void
print_counts(lines, words, chars, name)
	int64_t lines;
	int64_t words;
	int64_t chars;
	char *name;
{

	if (doline)
		(void)printf(" %7qd", (quad_t) lines);
	if (doword)
		(void)printf(" %7qd", (quad_t) words);
	if (dochar)
		(void)printf(" %7qd", (quad_t) chars);

	(void)printf(" %s\n", name);
}
