/*	$OpenBSD: lam.c,v 1.10 2003/12/09 00:55:18 mickey Exp $	*/
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
static char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lam.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: lam.c,v 1.10 2003/12/09 00:55:18 mickey Exp $";
#endif /* not lint */

/*
 *	lam - laminate files
 *	Author:  John Kunze, UCB
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	MAXOFILES	20
#define	BIGBUFSIZ	5 * BUFSIZ

struct	openfile {		/* open file structure */
	FILE	*fp;		/* file pointer */
	short	eof;		/* eof flag */
	short	pad;		/* pad flag for missing columns */
	char	eol;		/* end of line character */
	char	*sepstring;	/* string to print before each line */
	char	*format;	/* printf(3) style string spec. */
}	input[MAXOFILES];

int	morefiles;		/* set by getargs(), changed by gatherline() */
int	nofinalnl;		/* normally append \n to each output line */
char	line[BIGBUFSIZ];
char	*linep;

void	 error(char *, char *);
char	*gatherline(struct openfile *);
void	 getargs(char *[]);
char	*pad(struct openfile *);

int
main(int argc, char *argv[])
{
	struct	openfile *ip;

	getargs(argv);
	if (!morefiles)
		error("lam - laminate files", "");
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
getargs(char *av[])
{
	struct	openfile *ip = input;
	char *p;
	char *c;
	static char fmtbuf[BUFSIZ];
	char *fmtp = fmtbuf;
	int P, S, F, T;

	P = S = F = T = 0;		/* capitalized options */
	while ((p = *++av) != NULL) {
		if (*p != '-' || !p[1]) {
			morefiles++;
			if (*p == '-')
				ip->fp = stdin;
			else if ((ip->fp = fopen(p, "r")) == NULL)
				err(1, p);
			ip->pad = P;
			if (!ip->sepstring)
				ip->sepstring = (S ? (ip-1)->sepstring : "");
			if (!ip->format)
				ip->format = ((P || F) ? (ip-1)->format : "%s");
			if (!ip->eol)
				ip->eol = (T ? (ip-1)->eol : '\n');
			ip++;
			continue;
		}
		switch (*(c = ++p) | 040) {
		case 's':
			if (*++p || (p = *++av))
				ip->sepstring = p;
			else
				error("Need string after -%s", c);
			S = (*c == 'S' ? 1 : 0);
			break;
		case 't':
			if (*++p || (p = *++av))
				ip->eol = *p;
			else
				error("Need character after -%s", c);
			T = (*c == 'T' ? 1 : 0);
			nofinalnl = 1;
			break;
		case 'p':
			ip->pad = 1;
			P = (*c == 'P' ? 1 : 0);
		case 'f':
			F = (*c == 'F' ? 1 : 0);
			if (*++p || (p = *++av)) {
				fmtp += strlen(fmtp) + 1;
				if (fmtp >= fmtbuf + BUFSIZ)
					error("No more format space", "");
				snprintf(fmtp, fmtbuf + BUFSIZ - fmtp,
				    "%%%ss", p);
				ip->format = fmtp;
			}
			else
				error("Need string after -%s", c);
			break;
		default:
			error("What do you mean by -%s?", c);
			break;
		}
	}
	ip->fp = NULL;
	if (!ip->sepstring)
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
	char *end = s + BUFSIZ;
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
	n = snprintf(lp, line + sizeof line - lp, ip->format, s);
	lp += (n < line + sizeof(line) - lp) ? n : strlen(lp);
	return (lp);
}

void
error(char *msg, char *s)
{
	extern char *__progname;
	warnx(msg, s);
	fprintf(stderr,
	    "Usage: %s [ -[fp] min.max ] [ -s sepstring ] [ -t c ] file ...\n",
	    __progname);
	if (strncmp("lam - ", msg, 6) == 0)
		fprintf(stderr, "Options:\n"
		    "\t-f min.max\tfield widths for file fragments\n"
		    "\t-p min.max\tlike -f, but pad missing fragments\n"
		    "\t-s sepstring\tfragment separator\n"
"\t-t c\t\tinput line terminator is c, no \\n after output lines\n"
		    "\tCapitalized options affect more than one file.\n");
	exit(1);
}
