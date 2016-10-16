/*	$OpenBSD: ul.c,v 1.23 2016/10/16 11:28:54 jca Exp $	*/
/*	$NetBSD: ul.c,v 1.3 1994/12/07 00:28:24 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1993
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

#include <curses.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>
#include <wchar.h>

#define	IESC	L'\033'
#define	SO	L'\016'
#define	SI	L'\017'
#define	HFWD	'9'
#define	HREV	'8'
#define	FREV	'7'
#define	MAXBUF	512

#define	NORMAL	000
#define	ALTSET	001	/* Reverse */
#define	SUPERSC	002	/* Dim */
#define	SUBSC	004	/* Dim | Ul */
#define	UNDERL	010	/* Ul */
#define	BOLD	020	/* Bold */
#define	INDET	040	/* Indeterminate: either Bold or Ul */

int	must_use_uc, must_overstrike;
char	*CURS_UP, *CURS_RIGHT, *CURS_LEFT,
	*ENTER_STANDOUT, *EXIT_STANDOUT, *ENTER_UNDERLINE, *EXIT_UNDERLINE,
	*ENTER_DIM, *ENTER_BOLD, *ENTER_REVERSE, *UNDER_CHAR, *EXIT_ATTRIBUTES;

struct	CHAR	{
	char	c_mode;
	wchar_t	c_char;
	int	c_width;
	int	c_pos;
} ;

struct	CHAR	obuf[MAXBUF];
int	col, maxcol;
int	mode;
int	halfpos;
int	upln;
int	iflag;

int	outchar(int);
void	initcap(void);
void	initbuf(void);
void	mfilter(FILE *);
void	reverse(void);
void	fwd(void);
void	flushln(void);
void	msetmode(int);
void	outc(wchar_t, int);
void	overstrike(void);
void	iattr(void);

#define	PRINT(s) \
	do { \
		if (s) \
			tputs(s, 1, outchar); \
	} while (0)

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	int c;
	char *termtype;
	FILE *f;
	char termcap[1024];

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath tty", NULL) == -1)
		err(1, "pledge");

	termtype = getenv("TERM");
	if (termtype == NULL || (argv[0][0] == 'c' && !isatty(1)))
		termtype = "lpr";
	while ((c = getopt(argc, argv, "it:T:")) != -1)
		switch (c) {
		case 't':
		case 'T': /* for nroff compatibility */
			termtype = optarg;
			break;
		case 'i':
			iflag = 1;
			break;

		default:
			fprintf(stderr,
			    "usage: %s [-i] [-t terminal] [file ...]\n",
			    argv[0]);
			exit(1);
		}

	switch (tgetent(termcap, termtype)) {
	case 1:
		break;
	default:
		warnx("trouble reading termcap");
		/* FALLTHROUGH */
	case 0:
		/* No such terminal type - assume dumb */
		(void)strlcpy(termcap, "dumb:os:col#80:cr=^M:sf=^J:am:",
		    sizeof termcap);
		break;
	}
	initcap();
	if ((tgetflag("os") && ENTER_BOLD == NULL ) ||
	    (tgetflag("ul") && ENTER_UNDERLINE == NULL && UNDER_CHAR == NULL))
		must_overstrike = 1;
	initbuf();
	if (optind == argc)
		mfilter(stdin);
	else for (; optind<argc; optind++) {
		f = fopen(argv[optind],"r");
		if (f == NULL)
			err(1, "%s", argv[optind]);

		mfilter(f);
		fclose(f);
	}
	exit(0);
}

void
mfilter(FILE *f)
{
	struct CHAR	*cp;
	wint_t		 c;
	int		 skip_bs, w, wt;

	col = 1;
	skip_bs = 0;
	while (col < MAXBUF) {
		switch (c = fgetwc(f)) {
		case WEOF:
			/* Discard invalid bytes. */
			if (ferror(f)) {
				if (errno != EILSEQ)
					err(1, NULL);
				clearerr(f);
				break;
			}

			/* End of file. */
			if (maxcol)
				flushln();
			return;

		case L'\b':
			/*
			 * Back up one character position, not one
			 * display column, but ignore a second
			 * backspace after a double-width character.
			 */
			if (skip_bs > 0)
				skip_bs--;
			else if (col > 1)
				if (obuf[--col].c_width > 1)
					skip_bs = obuf[col].c_width - 1;
			continue;

		case L'\t':
			/* Calculate the target position. */
			wt = (obuf[col - 1].c_pos + 8) & ~7;

			/* Advance past known positions. */
			while ((w = obuf[col].c_pos) > 0 && w <= wt)
				col++;

			/* Advance beyond the end. */
			if (w == 0) {
				w = obuf[col - 1].c_pos;
				while (w < wt) {
					obuf[col].c_width = 1;
					obuf[col++].c_pos = ++w;
				}
			}
			if (col > maxcol)
				maxcol = col;
			break;

		case L'\r':
			col = 1;
			break;

		case SO:
			mode |= ALTSET;
			break;

		case SI:
			mode &= ~ALTSET;
			break;

		case IESC:
			switch (c = fgetwc(f)) {
			case HREV:
				if (halfpos == 0) {
					mode |= SUPERSC;
					halfpos--;
				} else if (halfpos > 0) {
					mode &= ~SUBSC;
					halfpos--;
				} else {
					halfpos = 0;
					reverse();
				}
				break;
			case HFWD:
				if (halfpos == 0) {
					mode |= SUBSC;
					halfpos++;
				} else if (halfpos < 0) {
					mode &= ~SUPERSC;
					halfpos++;
				} else {
					halfpos = 0;
					fwd();
				}
				break;
			case FREV:
				reverse();
				break;
			default:
				errx(1, "0%o: unknown escape sequence", c);
			}
			break;

		case L'_':
			if (obuf[col].c_char == L'\0') {
				obuf[col].c_char = L'_';
				obuf[col].c_width = 1;
			} else if (obuf[col].c_char == L'_') {
				if (obuf[col - 1].c_mode & UNDERL)
					obuf[col].c_mode |= UNDERL | mode;
				else if (obuf[col - 1].c_mode & BOLD)
					obuf[col].c_mode |= BOLD | mode;
				else
					obuf[col].c_mode |= INDET | mode;
			} else
				obuf[col].c_mode |= UNDERL | mode;
			/* FALLTHROUGH */

		case L' ':
			if (obuf[col].c_pos == 0) {
				obuf[col].c_width = 1;
				obuf[col].c_pos = obuf[col - 1].c_pos + 1;
			}
			col++;
			if (col > maxcol)
				maxcol = col;
			break;

		case L'\n':
			flushln();
			break;

		case L'\f':
			flushln();
			putwchar(L'\f');
			break;

		default:
			/* Discard valid, but non-printable characters. */
			if ((w = wcwidth(c)) == -1)
				break;

			if (obuf[col].c_char == L'\0') {
				obuf[col].c_char = c;
				obuf[col].c_mode = mode;
				obuf[col].c_width = w;
				obuf[col].c_pos = obuf[col - 1].c_pos + w;
			} else if (obuf[col].c_char == L'_') {
				obuf[col].c_char = c;
				obuf[col].c_mode |= UNDERL|mode;
				obuf[col].c_width = w;
				obuf[col].c_pos = obuf[col - 1].c_pos + w;
				for (cp = obuf + col; cp[1].c_pos > 0; cp++)
					cp[1].c_pos = cp[0].c_pos +
					    cp[1].c_width;
			} else if (obuf[col].c_char == c)
				obuf[col].c_mode |= BOLD|mode;
			else
				obuf[col].c_mode = mode;
			col++;
			if (col > maxcol)
				maxcol = col;
			break;
		}
		skip_bs = 0;
	}
}

void
flushln(void)
{
	int lastmode, i;
	int hadmodes = 0;

	for (i = maxcol; i > 0; i--) {
		if (obuf[i].c_mode & INDET) {
			obuf[i].c_mode &= ~INDET;
			if (i < maxcol && obuf[i + 1].c_mode & BOLD)
				obuf[i].c_mode |= BOLD;
			else
				obuf[i].c_mode |= UNDERL;
		}
	}

	lastmode = NORMAL;
	for (i = 1; i < maxcol; i++) {
		if (obuf[i].c_mode != lastmode) {
			hadmodes = 1;
			msetmode(obuf[i].c_mode);
			lastmode = obuf[i].c_mode;
		}
		if (obuf[i].c_char == L'\0') {
			if (upln)
				PRINT(CURS_RIGHT);
			else
				outc(L' ', 1);
		} else
			outc(obuf[i].c_char, obuf[i].c_width);
	}
	if (lastmode != NORMAL)
		msetmode(0);
	if (must_overstrike && hadmodes && !iflag)
		overstrike();
	putwchar(L'\n');
	if (iflag && hadmodes)
		iattr();
	(void)fflush(stdout);
	if (upln)
		upln--;
	initbuf();
}

/*
 * For terminals that can overstrike, overstrike underlines and bolds.
 * We don't do anything with halfline ups and downs, or Greek.
 */
void
overstrike(void)
{
	int i, j, needspace;

	putwchar(L'\r');
	needspace = 0;
	for (i = 1; i < maxcol; i++) {
		if (obuf[i].c_mode != UNDERL && obuf[i].c_mode != BOLD) {
			needspace += obuf[i].c_width;
			continue;
		}
		while (needspace > 0) {
			putwchar(L' ');
			needspace--;
		}
		if (obuf[i].c_mode == BOLD)
			putwchar(obuf[i].c_char);
		else
			for (j = 0; j < obuf[i].c_width; j++)
				putwchar(L'_');
	}
}

void
iattr(void)
{
	int i, j, needspace;
	char c;

	needspace = 0;
	for (i = 1; i < maxcol; i++) {
		switch (obuf[i].c_mode) {
		case NORMAL:
			needspace += obuf[i].c_width;
			continue;
		case ALTSET:
			c = 'g';
			break;
		case SUPERSC:
			c = '^';
			break;
		case SUBSC:
			c = 'v';
			break;
		case UNDERL:
			c = '_';
			break;
		case BOLD:
			c = '!';
			break;
		default:
			c = 'X';
			break;
		}
		while (needspace > 0) {
			putwchar(L' ');
			needspace--;
		}
		for (j = 0; j < obuf[i].c_width; j++)
			putwchar(c);
	}
	putwchar(L'\n');
}

void
initbuf(void)
{
	bzero(obuf, sizeof (obuf));	/* depends on NORMAL == 0 */
	col = 1;
	maxcol = 0;
	mode &= ALTSET;
}

void
fwd(void)
{
	int oldcol, oldmax;

	oldcol = col;
	oldmax = maxcol;
	flushln();
	col = oldcol;
	maxcol = oldmax;
}

void
reverse(void)
{
	upln++;
	fwd();
	PRINT(CURS_UP);
	PRINT(CURS_UP);
	upln++;
}

void
initcap(void)
{
	static char tcapbuf[512];
	char *bp = tcapbuf;

	/* This nonsense attempts to work with both old and new termcap */
	CURS_UP =		tgetstr("up", &bp);
	CURS_RIGHT =		tgetstr("ri", &bp);
	if (CURS_RIGHT == NULL)
		CURS_RIGHT =	tgetstr("nd", &bp);
	CURS_LEFT =		tgetstr("le", &bp);
	if (CURS_LEFT == NULL)
		CURS_LEFT =	tgetstr("bc", &bp);
	if (CURS_LEFT == NULL && tgetflag("bs"))
		CURS_LEFT =	"\b";

	ENTER_STANDOUT =	tgetstr("so", &bp);
	EXIT_STANDOUT =		tgetstr("se", &bp);
	ENTER_UNDERLINE =	tgetstr("us", &bp);
	EXIT_UNDERLINE =	tgetstr("ue", &bp);
	ENTER_DIM =		tgetstr("mh", &bp);
	ENTER_BOLD =		tgetstr("md", &bp);
	ENTER_REVERSE =		tgetstr("mr", &bp);
	EXIT_ATTRIBUTES =	tgetstr("me", &bp);

	if (!ENTER_BOLD && ENTER_REVERSE)
		ENTER_BOLD = ENTER_REVERSE;
	if (!ENTER_BOLD && ENTER_STANDOUT)
		ENTER_BOLD = ENTER_STANDOUT;
	if (!ENTER_UNDERLINE && ENTER_STANDOUT) {
		ENTER_UNDERLINE = ENTER_STANDOUT;
		EXIT_UNDERLINE = EXIT_STANDOUT;
	}
	if (!ENTER_DIM && ENTER_STANDOUT)
		ENTER_DIM = ENTER_STANDOUT;
	if (!ENTER_REVERSE && ENTER_STANDOUT)
		ENTER_REVERSE = ENTER_STANDOUT;
	if (!EXIT_ATTRIBUTES && EXIT_STANDOUT)
		EXIT_ATTRIBUTES = EXIT_STANDOUT;
	
	/*
	 * Note that we use REVERSE for the alternate character set,
	 * not the as/ae capabilities.  This is because we are modelling
	 * the model 37 teletype (since that's what nroff outputs) and
	 * the typical as/ae is more of a graphics set, not the greek
	 * letters the 37 has.
	 */

	UNDER_CHAR =		tgetstr("uc", &bp);
	must_use_uc = (UNDER_CHAR && !ENTER_UNDERLINE);
}

int
outchar(int c)
{
	return (putwchar(c) != WEOF ? c : EOF);
}

static int curmode = 0;

void
outc(wchar_t c, int width)
{
	int i;

	putwchar(c);
	if (must_use_uc && (curmode&UNDERL)) {
		for (i = 0; i < width; i++)
			PRINT(CURS_LEFT);
		for (i = 0; i < width; i++)
			PRINT(UNDER_CHAR);
	}
}

void
msetmode(int newmode)
{
	if (!iflag) {
		if (curmode != NORMAL && newmode != NORMAL)
			msetmode(NORMAL);
		switch (newmode) {
		case NORMAL:
			switch(curmode) {
			case NORMAL:
				break;
			case UNDERL:
				PRINT(EXIT_UNDERLINE);
				break;
			default:
				/* This includes standout */
				PRINT(EXIT_ATTRIBUTES);
				break;
			}
			break;
		case ALTSET:
			PRINT(ENTER_REVERSE);
			break;
		case SUPERSC:
			/*
			 * This only works on a few terminals.
			 * It should be fixed.
			 */
			PRINT(ENTER_UNDERLINE);
			PRINT(ENTER_DIM);
			break;
		case SUBSC:
			PRINT(ENTER_DIM);
			break;
		case UNDERL:
			PRINT(ENTER_UNDERLINE);
			break;
		case BOLD:
			PRINT(ENTER_BOLD);
			break;
		default:
			/*
			 * We should have some provision here for multiple modes
			 * on at once.  This will have to come later.
			 */
			PRINT(ENTER_STANDOUT);
			break;
		}
	}
	curmode = newmode;
}
