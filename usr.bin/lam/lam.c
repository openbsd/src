/*	$OpenBSD: lam.c,v 1.12 2004/12/27 23:37:37 deraadt Exp $	*/
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)lam.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] = "$OpenBSD: lam.c,v 1.12 2004/12/27 23:37:37 deraadt Exp $";
#endif /* not lint */

/*
 *	lam - laminate files
 *	Author:  John Kunze, UCB
 */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	BIGBUFSIZ	5 * BUFSIZ

struct	openfile {		/* open file structure */
	FILE	*fp;		/* file pointer */
	short	eof;		/* eof flag */
	short	pad;		/* pad flag for missing columns */
	char	eol;		/* end of line character */
	char	*sepstring;	/* string to print before each line */
	char	*format;	/* printf(3) style string spec. */
}	input[OPEN_MAX];

int	morefiles;		/* set by getargs(), changed by gatherline() */
int	nofinalnl;		/* normally append \n to each output line */
char	line[BIGBUFSIZ];
char	*linep;

void	 usage(void);
char	*gatherline(struct openfile *);
void	 getargs(int, char *[]);
char	*pad(struct openfile *);

int
main(int argc, char *argv[])
{
	struct	openfile *ip;

	getargs(argc, argv);
	if (!morefiles)
		usage();
	for (;;) {
		linep = line;
		for (ip = input; ip->fp != NULL; ip++)
			linep = gatherline(ip);
		if (!morefiles)
			exit(0);
		fputs(line, stdout);
		fputs(ip->sepstring, stdout);
		if (!nofinalnl)
			putchar('\n');
	}
}

void
getargs(int argc, char *argv[])
{
	struct openfile *ip = input;
	char *p;
	int ch, P, S, F, T;
	size_t siz;

	P = S = F = T = 0;		/* capitalized options */
	while (optind < argc) {
		switch (ch = getopt(argc, argv, "F:f:P:p:S:s:T:t:")) {
		case 'F': case 'f':
			F = (ch == 'F');
			/* Validate format string argument. */
			for (p = optarg; *p != '\0'; p++)
				if (!isdigit(*p) && *p != '.' && *p != '-')
					errx(1, "%s: invalid width specified",
					     optarg);
			/* '%' + width + 's' + '\0' */
			siz = p - optarg + 3;
			if ((p = realloc(ip->format, siz)) == NULL)
				err(1, NULL);
			snprintf(p, siz, "%%%ss", optarg);
			ip->format = p;
			break;
		case 'P': case 'p':
			P = (ch == 'P');
			ip->pad = 1;
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
			morefiles++;
			if (strcmp(argv[optind], "-") == 0)
				ip->fp = stdin;
			else if ((ip->fp = fopen(argv[optind], "r")) == NULL)
				err(1, "%s", argv[optind]);
			ip->pad = P;
			if (ip->sepstring == NULL)
				ip->sepstring = S ? (ip-1)->sepstring : "";
			if (ip->format == NULL)
				ip->format = (P || F) ? (ip-1)->format : "%s";
			if (ip->eol == '\0')
				ip->eol = T ? (ip-1)->eol : '\n';
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

	n = strlcpy(lp, ip->sepstring,  line + sizeof(line) - lp);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	if (ip->pad) {
		n = snprintf(lp, line + sizeof(line) - lp, ip->format, "");
		if (n > 0)
			lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	}
	return (lp);
}

char *
gatherline(struct openfile *ip)
{
	size_t n;
	char s[BUFSIZ];
	char *p;
	char *lp = linep;
	char *end = s + BUFSIZ - 1;
	int c;

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
		morefiles--;
		return (pad(ip));
	}
	n = strlcpy(lp, ip->sepstring, line + sizeof(line) - lp);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	n = snprintf(lp, line + sizeof(line) - lp, ip->format, s);
	if (n > 0)
		lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
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
