/*	$OpenBSD: wc.c,v 1.25 2018/09/30 12:44:22 schwarze Exp $	*/

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

#include <sys/param.h>	/* MAXBSIZE */
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <util.h>
#include <wchar.h>
#include <wctype.h>

int64_t	tlinect, twordct, tcharct;
int	doline, doword, dochar, humanchar, multibyte;
int	rval;
extern char *__progname;

static void print_counts(int64_t, int64_t, int64_t, char *);
static void format_and_print(int64_t);
static void cnt(char *);

int
main(int argc, char *argv[])
{
	int ch;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "lwchm")) != -1)
		switch(ch) {
		case 'l':
			doline = 1;
			break;
		case 'w':
			doword = 1;
			break;
		case 'm':
			if (MB_CUR_MAX > 1)
				multibyte = 1;
			/* FALLTHROUGH */
		case 'c':
			dochar = 1;
			break;
		case 'h':
			humanchar = 1;
			break;
		case '?':
		default:
			fprintf(stderr,
			    "usage: %s [-c | -m] [-hlw] [file ...]\n",
			    __progname);
			return 1;
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
		cnt(NULL);
	} else {
		int dototal = (argc > 1);

		do {
			cnt(*argv);
		} while(*++argv);

		if (dototal)
			print_counts(tlinect, twordct, tcharct, "total");
	}

	return rval;
}

static void
cnt(char *file)
{
	static char *buf;
	static size_t bufsz;

	FILE *stream;
	char *C;
	wchar_t wc;
	short gotsp;
	ssize_t len;
	int64_t linect, wordct, charct;
	struct stat sbuf;
	int fd;

	linect = wordct = charct = 0;
	stream = NULL;
	if (file) {
		if ((fd = open(file, O_RDONLY, 0)) < 0) {
			warn("%s", file);
			rval = 1;
			return;
		}
	} else  {
		fd = STDIN_FILENO;
	}

	if (!doword && !multibyte) {
		if (bufsz < MAXBSIZE &&
		    (buf = realloc(buf, MAXBSIZE)) == NULL)
			err(1, NULL);
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
		if (file == NULL)
			stream = stdin;
		else if ((stream = fdopen(fd, "r")) == NULL) {
			warn("%s", file);
			close(fd);
			rval = 1;
			return;
		}

		/*
		 * Do it the hard way.
		 * According to POSIX, a word is a "maximal string of
		 * characters delimited by whitespace."  Nothing is said
		 * about a character being printing or non-printing.
		 */
		gotsp = 1;
		while ((len = getline(&buf, &bufsz, stream)) > 0) {
			if (multibyte) {
				const char *end = buf + len;
				for (C = buf; C < end; C += len) {
					++charct;
					len = mbtowc(&wc, C, MB_CUR_MAX);
					if (len == -1) {
						mbtowc(NULL, NULL,
						    MB_CUR_MAX);
						len = 1;
						wc = L'?';
					} else if (len == 0)
						len = 1;
					if (iswspace(wc)) {
						gotsp = 1;
						if (wc == L'\n')
							++linect;
					} else if (gotsp) {
						gotsp = 0;
						++wordct;
					}
				}
			} else {
				charct += len;
				for (C = buf; len--; ++C) {
					if (isspace((unsigned char)*C)) {
						gotsp = 1;
						if (*C == '\n')
							++linect;
					} else if (gotsp) {
						gotsp = 0;
						++wordct;
					}
				}
			}
		}
		if (ferror(stream)) {
			warn("%s", file);
			rval = 1;
		}
	}

	print_counts(linect, wordct, charct, file);

	/*
	 * Don't bother checking doline, doword, or dochar -- speeds
	 * up the common case
	 */
	tlinect += linect;
	twordct += wordct;
	tcharct += charct;

	if ((stream == NULL ? close(fd) : fclose(stream)) != 0) {
		warn("%s", file);
		rval = 1;
	}
}

static void
format_and_print(int64_t v)
{
	if (humanchar) {
		char result[FMT_SCALED_STRSIZE];

		fmt_scaled((long long)v, result);
		printf("%7s", result);
	} else {
		printf(" %7lld", v);
	}
}

static void
print_counts(int64_t lines, int64_t words, int64_t chars, char *name)
{
	if (doline)
		format_and_print(lines);
	if (doword)
		format_and_print(words);
	if (dochar)
		format_and_print(chars);

	if (name)
		printf(" %s\n", name);
	else
		printf("\n");
}
