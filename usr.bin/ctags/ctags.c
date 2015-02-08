/*	$OpenBSD: ctags.c,v 1.15 2015/02/08 23:40:34 deraadt Exp $	*/
/*	$NetBSD: ctags.c,v 1.4 1995/09/02 05:57:23 jtc Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ctags.h"

/*
 * ctags: create a tags file
 */

NODE	*head;			/* head of the sorted binary tree */

				/* boolean "func" (see init()) */
bool	_wht[256], _itk[256], _btk[256];

FILE	*inf;			/* ioptr for current input file */
FILE	*outf;			/* ioptr for tags file */

long	lineftell;		/* ftell after getc( inf ) == '\n' */

int	lineno;			/* line number of current line */
int	dflag;			/* -d: non-macro defines */
int	vflag;			/* -v: vgrind style index output */
int	wflag;			/* -w: suppress warnings */
int	xflag;			/* -x: cxref style output */

char	*curfile;		/* current input file name */
char	searchar = '/';		/* use /.../ searches by default */
char	lbuf[LINE_MAX];

void	init(void);
void	find_entries(char *);

int
main(int argc, char *argv[])
{
	static char	*outfile = "tags";	/* output file */
	int	aflag;				/* -a: append to tags */
	int	uflag;				/* -u: update tags */
	int	exit_val;			/* exit value */
	int	step;				/* step through args */
	int	ch;				/* getopts char */
	char	*cmd;

	aflag = uflag = NO;
	while ((ch = getopt(argc, argv, "BFadf:tuwvx")) != -1)
		switch(ch) {
		case 'B':
			searchar = '?';
			break;
		case 'F':
			searchar = '/';
			break;
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			outfile = optarg;
			break;
		case 't':
			/* backwards compatibility */
			break;
		case 'u':
			uflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case 'v':
			vflag = 1;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			goto usage;
		}
	argv += optind;
	argc -= optind;
	if (!argc) {
usage:		(void)fprintf(stderr,
			"usage: ctags [-aBdFuvwx] [-f tagsfile] file ...\n");
		exit(1);
	}

	init();

	for (exit_val = step = 0; step < argc; ++step)
		if (!(inf = fopen(argv[step], "r"))) {
			warn("%s", argv[step]);
			exit_val = 1;
		}
		else {
			curfile = argv[step];
			find_entries(argv[step]);
			(void)fclose(inf);
		}

	if (head) {
		if (xflag)
			put_entries(head);
		else {
			if (uflag) {
				for (step = 0; step < argc; step++) {
					if (asprintf(&cmd,
					    "mv %s OTAGS; fgrep -v '\t%s\t' OTAGS >%s; rm OTAGS",
					    outfile, argv[step], outfile) == -1)
						err(1, "out of space");
					system(cmd);
					free(cmd);
					cmd = NULL;
				}
				aflag = 1;
			}
			if (!(outf = fopen(outfile, aflag ? "a" : "w")))
				err(exit_val, "%s", outfile);
			put_entries(head);
			(void)fclose(outf);
			if (uflag) {
				if (asprintf(&cmd, "sort -o %s %s",
				    outfile, outfile) == -1)
						err(1, "out of space");
				system(cmd);
				free(cmd);
				cmd = NULL;
			}
		}
	}
	exit(exit_val);
}

/*
 * init --
 *	this routine sets up the boolean psuedo-functions which work by
 *	setting boolean flags dependent upon the corresponding character.
 *	Every char which is NOT in that string is false with respect to
 *	the pseudo-function.  Therefore, all of the array "_wht" is NO
 *	by default and then the elements subscripted by the chars in
 *	CWHITE are set to YES.  Thus, "_wht" of a char is YES if it is in
 *	the string CWHITE, else NO.
 */
void
init(void)
{
	int		i;
	unsigned char	*sp;

	for (i = 0; i < 256; i++)
		_wht[i] = _itk[i] = _btk[i] = NO;
#define	CWHITE	" \f\t\n"
	for (sp = CWHITE; *sp; sp++)	/* white space chars */
		_wht[*sp] = YES;
#define	CINTOK	"ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz0123456789"
	for (sp = CINTOK; *sp; sp++)	/* valid in-token chars */
		_itk[*sp] = YES;
#define	CBEGIN	"ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"
	for (sp = CBEGIN; *sp; sp++)	/* token starting chars */
		_btk[*sp] = YES;
}

/*
 * find_entries --
 *	this routine opens the specified file and calls the function
 *	which searches the file.
 */
void
find_entries(char *file)
{
	char	*cp;

	lineno = 0;				/* should be 1 ?? KB */
	if ((cp = strrchr(file, '.'))) {
		if (cp[1] == 'l' && !cp[2]) {
			int	c;

			for (;;) {
				if (GETC(==, EOF))
					return;
				if (!iswhite(c)) {
					rewind(inf);
					break;
				}
			}
#define	LISPCHR	";(["
/* lisp */		if (strchr(LISPCHR, c)) {
				l_entries();
				return;
			}
/* lex */		else {
				/*
				 * we search all 3 parts of a lex file
				 * for C references.  This may be wrong.
				 */
				toss_yysec();
				(void)strlcpy(lbuf, "%%$", sizeof lbuf);
				pfnote("yylex", lineno);
				rewind(inf);
			}
		}
/* yacc */	else if (cp[1] == 'y' && !cp[2]) {
			/*
			 * we search only the 3rd part of a yacc file
			 * for C references.  This may be wrong.
			 */
			toss_yysec();
			(void)strlcpy(lbuf, "%%$", sizeof lbuf);
			pfnote("yyparse", lineno);
			y_entries();
		}
/* fortran */	else if ((cp[1] != 'c' && cp[1] != 'h') && !cp[2]) {
			if (PF_funcs())
				return;
			rewind(inf);
		}
	}
/* C */	c_entries();
}
