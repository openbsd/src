/*	$OpenBSD: subs.c,v 1.7 1998/11/29 19:45:10 pjanzen Exp $	*/

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
#if 0
static char sccsid[] = "@(#)subs.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: subs.c,v 1.7 1998/11/29 19:45:10 pjanzen Exp $";
#endif
#endif /* not lint */

#include "back.h"

int     buffnum;
char    outbuff[BUFSIZ];

static char plred[] = "Player is red, computer is white.";
static char plwhite[] = "Player is white, computer is red.";
static char nocomp[] = "(No computer play.)";

char   *descr[] = {
	"Usage:  backgammon [-] [-nrwb] [-p [r|w|b]] [-t <term>] [-s <file>]\n",
	"\t-h\tget this list\n\t-n\tdon't ask for rules or instructions",
	"\t-r\tplayer is red (implies n)\n\t-w\tplayer is white (implies n)",
	"\t-b\ttwo players, red and white (implies n)",
	"\t-p r\tprint the board before red's turn",
	"\t-p w\tprint the board before white's turn",
	"\t-p b\tprint the board before all turns",
	"\t-t term\tterminal is type term",
	"\t-s file\trecover previously saved game from file",
	0
};

void
errexit(s)
	const char *s;
{
	write(2, "\n", 1);
	perror(s);
	getout(0);
}

int
addbuf(c)
	int     c;
{
	buffnum++;
	if (buffnum == BUFSIZ) {
		if (write(1, outbuff, BUFSIZ) != BUFSIZ)
			errexit("addbuf (write):");
		buffnum = 0;
	}
	outbuff[buffnum] = c;
	return(0);
}

void
buflush()
{
	if (buffnum < 0)
		return;
	buffnum++;
	if (write(1, outbuff, buffnum) != buffnum)
		errexit("buflush (write):");
	buffnum = -1;
}

int
readc()
{
	char    c;

	if (tflag) {
		cline();
		newpos();
	}
	buflush();
	if (read(0, &c, 1) != 1)
		errexit("readc");
	if (c == '\004')	/* ^D	*/
		getout(0);
	if (c == '\033' || c == '\015')
		return('\n');
	if (cflag)
		return(c);
	if (c == '\014')
		return('R');
	if (c >= 'a' && c <= 'z')
		return(c & 0137);
	return(c);
}

void
writec(c)
	char    c;
{
	if (tflag)
		fancyc(c);
	else
		addbuf(c);
}

void
writel(l)
	char   *l;
{
#ifdef DEBUG
	char   *s;

	if (trace == NULL)
		trace = fopen("bgtrace", "w");

	fprintf(trace, "writel: \"");
	for (s = l; *s; s++) {
		if (*s < ' ' || *s == '\177')
			fprintf(trace, "^%c", (*s)^0100);
		else
			putc(*s, trace);
	}
	fprintf(trace, "\"\n");
	fflush(trace);
#endif

	while (*l)
		writec(*l++);
}

void
proll()
{
	if (d0)
		swap;
	if (cturn == 1)
		writel("Red's roll:  ");
	else
		writel("White's roll:  ");
	writec(D0 + '0');
	writec('\040');
	writec(D1 + '0');
	if (tflag)
		cline();
}

void
wrint(n)
	int     n;
{
	int     i, j, t;

	for (i = 4; i > 0; i--) {
		t = 1;
		for (j = 0; j < i; j++)
			t *= 10;
		if (n > t - 1)
			writec((n / t) % 10 + '0');
	}
	writec(n % 10 + '0');
}

void
gwrite()
{
	int     r, c;

	if (tflag) {
		r = curr;
		c = curc;
		curmove(16, 0);
	}
	if (gvalue > 1) {
		writel("Game value:  ");
		wrint(gvalue);
		writel(".  ");
		if (dlast == -1)
			writel(color[0]);
		else
			writel(color[1]);
		writel(" doubled last.");
	} else {
		switch (pnum) {
		case -1:	/* player is red */
			writel(plred);
			break;
		case 0:	/* player is both colors */
			writel(nocomp);
			break;
		case 1:	/* player is white */
			writel(plwhite);
		}
	}
	if (rscore || wscore) {
		writel("  ");
		wrscore();
	}
	if (tflag) {
		cline();
		curmove(r, c);
	}
}

int
quit()
{
	if (tflag) {
		curmove(20, 0);
		clend();
	} else
		writec('\n');
	writel("Are you sure you want to quit?");
	if (yorn(0)) {
		if (rfl) {
			writel("Would you like to save this game?");
			if (yorn(0))
				save(0);
		}
		cturn = 0;
		return(1);
	}
	return(0);
}

int
yorn(special)
	char    special;	/* special response */
{
	char    c;
	int     i;

	i = 1;
	while ((c = readc()) != 'Y' && c != 'N') {
		if (special && c == special)
			return(2);
		if (i) {
			if (special) {
				writel("  (Y, N, or ");
				writec(special);
				writec(')');
			} else
				writel("  (Y or N)");
			i = 0;
		} else
			writec('\007');
	}
	if (c == 'Y')
		writel("  Yes.\n");
	else
		writel("  No.\n");
	if (tflag)
		buflush();
	return(c == 'Y');
}

void
wrhit(i)
	int     i;
{
	writel("Blot hit on ");
	wrint(i);
	writec('.');
	writec('\n');
}

void
nexturn()
{
	int     c;

	cturn = -cturn;
	c = cturn / abs(cturn);
	home = bar;
	bar = 25 - bar;
	offptr += c;
	offopp -= c;
	inptr += c;
	inopp -= c;
	Colorptr += c;
	colorptr += c;
}

void
getarg(argc,argv)
	int     argc;
	char  **argv;
{
	int     ch;
	int     j;

	while ((ch = getopt(argc, argv, "bhnp:rs:t:w")) != -1)
		switch((char)ch) {
		case 'n':	/* don't ask if rules or instructions needed */
			if (rflag)
				break;
			aflag = 0;
			args[acnt++] = 'n';
			break;

		case 'b':	/* player is both red and white */
			if (rflag)
				break;
			pnum = 0;
			aflag = 0;
			args[acnt++] = 'b';
			break;

		case 'r':	/* player is red */
			if (rflag)
				break;
			pnum = -1;
			aflag = 0;
			args[acnt++] = 'r';
			break;

		case 'w':	/* player is white */
			if (rflag)
				break;
			pnum = 1;
			aflag = 0;
			args[acnt++] = 'w';
			break;

		case 't':	/* use spec'd term from /etc/termcap */
			tflag = getcaps(optarg);
			break;

		case 's':	/* restore saved game */
			recover(optarg);
			break;

		case 'p':	/* print board after move */
			switch(optarg[0]) {
				case 'r':	bflag = 1;
						break;
				case 'w':	bflag = -1;
						break;
				case 'b':
				default:	bflag = 0;
						break;
			}
			break;

		default:	/* print cmdline options */
		case 'h':
			for (j = 0; descr[j] != NULL; j++)
				printf("%s\n", descr[j]);
			exit(0);
			break;
	} /* end switch */
}

void
init()
{
	int     i;

	for (i = 0; i < 26;)
		board[i++] = 0;
	board[1] = 2;
	board[6] = board[13] = -5;
	board[8] = -3;
	board[12] = board[19] = 5;
	board[17] = 3;
	board[24] = -2;
	off[0] = off[1] = -15;
	in[0] = in[1] = 5;
	gvalue = 1;
	dlast = 0;
}

void
wrscore()
{
	writel("Score:  ");
	writel(color[1]);
	writec(' ');
	wrint(rscore);
	writel(", ");
	writel(color[0]);
	writec(' ');
	wrint(wscore);
}

void
fixtty(t)
	struct termios *t;
{
	if (tflag)
		newpos();
	buflush();
	if (tcsetattr(0, TCSADRAIN, t) < 0)
		errexit("fixtty");
}

void
getout(dummy)
	int     dummy;
{
	/* go to bottom of screen */
	if (tflag) {
		curmove(23, 0);
		cline();
	} else
		writec('\n');

	/* fix terminal status */
	fixtty(&old);
	exit(0);
}

void
roll()
{
	char    c;
	int     row;
	int     col;

	if (iroll) {
		if (tflag) {
			row = curr;
			col = curc;
			curmove(17, 0);
		} else
			writec('\n');
		writel("ROLL: ");
		c = readc();
		if (c != '\n') {
			while (c < '1' || c > '6')
				c = readc();
			D0 = c - '0';
			writec(' ');
			writec(c);
			c = readc();
			while (c < '1' || c > '6')
				c = readc();
			D1 = c - '0';
			writec(' ');
			writec(c);
			if (tflag) {
				curmove(17, 0);
				cline();
				curmove(row, col);
			} else
				writec('\n');
			return;
		}
		if (tflag) {
			curmove(17, 0);
			cline();
			curmove(row, col);
		} else
			writec('\n');
	}
	D0 = rnum(6) + 1;
	D1 = rnum(6) + 1;
	d0 = 0;
}
