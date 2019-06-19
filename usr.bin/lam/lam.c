/*	$OpenBSD: lam.c,v 1.22 2018/07/29 11:27:14 schwarze Exp $	*/
/*	$NetBSD: lam.c,v 1.2 1994/11/14 20:27:42 jtc Exp $	*/

/*-
 * Copyright (c) 1993
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

/*
 *	lam - laminate files
 *	Author:  John Kunze, UCB
 */

#include <sys/param.h>	/* NOFILE_MAX */

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	BIGBUFSIZ	5 * BUFSIZ

struct	openfile {		/* open file structure */
	FILE	*fp;		/* file pointer */
	int	minwidth;	/* pad this column to this width */
	int	maxwidth;	/* truncate this column */
	short	eof;		/* eof flag */
	short	pad;		/* pad flag for missing columns */
	char	eol;		/* end of line character */
	char	align;		/* '0' for zero fill, '-' for left align */
	char	*sepstring;	/* string to print before each line */
}	input[NOFILE_MAX + 1];	/* last one is for the last -s arg. */
#define INPUTSIZE sizeof(input) / sizeof(*input)

int	numfiles;		/* number of open files */
int	nofinalnl;		/* normally append \n to each output line */
char	line[BIGBUFSIZ];
char	*linep;

int	 mbswidth_truncate(char *, int);  /* utf8.c */

void	 usage(void);
char	*gatherline(struct openfile *);
void	 getargs(int, char *[]);
char	*pad(struct openfile *);

int
main(int argc, char *argv[])
{
	int i;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	/* Process arguments, set numfiles to file argument count. */
	getargs(argc, argv);
	if (numfiles == 0)
		usage();

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* Concatenate lines from each file, then print. */
	for (;;) {
		linep = line;
		/*
		 * For each file that has a line to print, numfile is
		 * incremented.  Thus if numfiles is 0, we are done.
		 */
		numfiles = 0;
		for (i = 0; i < INPUTSIZE - 1 && input[i].fp != NULL; i++)
			linep = gatherline(&input[i]);
		if (numfiles == 0)
			exit(0);
		fputs(line, stdout);
		/* Print terminating -s argument. */
		fputs(input[i].sepstring, stdout);
		if (!nofinalnl)
			putchar('\n');
	}
}

void
getargs(int argc, char *argv[])
{
	struct openfile *ip = input;
	const char *errstr;
	char *p, *q;
	int ch, P, S, F, T;

	P = S = F = T = 0;		/* capitalized options */
	while (optind < argc) {
		switch (ch = getopt(argc, argv, "F:f:P:p:S:s:T:t:")) {
		case 'P': case 'p':
			P = (ch == 'P');
			ip->pad = 1;
			/* FALLTHROUGH */
		case 'F': case 'f':
			F = (ch == 'F');
			/* Validate format string argument. */
			p = optarg;
			if (*p == '0' || *p == '-')
				ip->align = *p++;
			else
				ip->align = ' ';
			if ((q = strchr(p, '.')) != NULL)
				*q++ = '\0';
			if (*p != '\0') {
				ip->minwidth = strtonum(p, 1, INT_MAX,
				    &errstr);
				if (errstr != NULL)
					errx(1, "minimum width is %s: %s",
					    errstr, p);
			}
			if (q != NULL) {
				ip->maxwidth = strtonum(q, 1, INT_MAX,
				    &errstr);
				if (errstr != NULL)
					errx(1, "maximum width is %s: %s",
					    errstr, q);
			} else
				ip->maxwidth = INT_MAX;
			break;
		case 'S': case 's':
			S = (ch == 'S');
			ip->sepstring = optarg;
			break;
		case 'T': case 't':
			T = (ch == 'T');
			if (strlen(optarg) != 1)
				usage();
			ip->eol = optarg[0];
			nofinalnl = 1;
			break;
		case -1:
			if (optind >= argc)
				break;		/* to support "--" */
			/* This is a file, not a flag. */
			++numfiles;
			if (numfiles >= INPUTSIZE)
				errx(1, "too many files");
			if (strcmp(argv[optind], "-") == 0)
				ip->fp = stdin;
			else if ((ip->fp = fopen(argv[optind], "r")) == NULL)
				err(1, "%s", argv[optind]);
			ip->pad = P;
			if (ip->sepstring == NULL)
				ip->sepstring = S ? (ip-1)->sepstring : "";
			if (ip->eol == '\0')
				ip->eol = T ? (ip-1)->eol : '\n';
			if (ip->align == '\0') {
				if (F || P) {
					ip->align = (ip-1)->align;
					ip->minwidth = (ip-1)->minwidth;
					ip->maxwidth = (ip-1)->maxwidth;
				} else
					ip->maxwidth = INT_MAX;
			}
			ip++;
			optind++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	ip->fp = NULL;
	if (ip->sepstring == NULL)
		ip->sepstring = "";
}

char *
pad(struct openfile *ip)
{
	size_t n;
	char *lp = linep;
	int i = 0;

	n = strlcpy(lp, ip->sepstring,  line + sizeof(line) - lp);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	if (ip->pad)
		while (i++ < ip->minwidth && lp + 1 < line + sizeof(line))
			*lp++ = ' ';
	*lp = '\0';
	return (lp);
}

/*
 * Grab line from file, appending to linep.  Increments numfiles if file
 * is still open.
 */
char *
gatherline(struct openfile *ip)
{
	size_t n;
	char s[BUFSIZ];
	char *p;
	char *lp = linep;
	char *end = s + BUFSIZ - 1;
	int c, width;

	if (ip->eof)
		return (pad(ip));
	for (p = s; (c = fgetc(ip->fp)) != EOF && p < end; p++)
		if ((*p = c) == ip->eol)
			break;
	*p = '\0';
	if (c == EOF) {
		ip->eof = 1;
		if (ip->fp == stdin)
			fclose(stdin);
		return (pad(ip));
	}
	/* Something will be printed. */
	numfiles++;
	n = strlcpy(lp, ip->sepstring, line + sizeof(line) - lp);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	width = mbswidth_truncate(s, ip->maxwidth);
	if (ip->align != '-')
		while (width++ < ip->minwidth && lp + 1 < line + sizeof(line))
			*lp++ = ip->align;
	n = strlcpy(lp, s, line + sizeof(line) - lp);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	if (ip->align == '-')
		while (width++ < ip->minwidth && lp + 1 < line + sizeof(line))
			*lp++ = ' ';
	*lp = '\0';
	return (lp);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-f min.max] [-p min.max] [-s sepstring] [-t c] file ...\n",
	    __progname);
	exit(1);
}
