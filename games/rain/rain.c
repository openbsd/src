/*	$OpenBSD: rain.c,v 1.6 1998/08/19 07:41:06 pjanzen Exp $	*/

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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rain.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: rain.c,v 1.6 1998/08/19 07:41:06 pjanzen Exp $";
#endif
#endif /* not lint */

/*
 * rain 11/3/1980 EPS/CITHEP
 * cc rain.c -o rain -O -ltermlib
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#define	cursor(c, r)	tputs(tgoto(CM, c, r), 1, fputchar)

static struct termios sg, old_tty;

int	fputchar __P((int));
static void	onsig __P((int));


char	*LL, *TE, *VE;

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern speed_t ospeed;
	extern char *UP;
	register int x, y, j;
	register char *CM, *BC, *DN, *ND, *term;
	char *TI, *tcp, *mp, tcb[100];
	long tcols, tlines;
	u_int delay = 0;
	char ch;
	int xpos[5], ypos[5];
#ifdef TIOCGWINSZ
	struct winsize ws;
#endif

	/* revoke */
	setegid(getgid());
	setgid(getgid());

	while ((ch = getopt(argc, argv, "d:h")) != -1)
		switch(ch) {
		case 'd':
			if ((delay = (u_int)strtoul(optarg,(char **)NULL,10)) < 1
				|| delay > 1000)
				errx(1, "invalid delay (1-1000)");
			delay *= 1000;  /* ms -> us */
			break;
		case 'h':
		default:
			(void)fprintf(stderr,"usage: rain [-d delay]\n");
			exit(1);
		}

	if (!(term = getenv("TERM")))
		errx(1, "TERM: parameter not set");
	if (!(mp = malloc((u_int)1024)))
		errx(1, "out of space");
	if (tgetent(mp, term) <= 0)
		errx(1, "unknown terminal type `%s'", term);
	tcp = tcb;
	if (!(CM = tgetstr("cm", &tcp)))
		errx(1, "terminal not capable of cursor motion");
	if (!(BC = tgetstr("bc", &tcp)))
		BC = "\b";
	if (!(DN = tgetstr("dn", &tcp)))
		DN = "\n";
	if (!(ND = tgetstr("nd", &tcp)))
		ND = " ";
	VE = tgetstr("ve", &tcp);
#ifdef TIOCGWINSZ
	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) != -1 &&
	    ws.ws_col && ws.ws_row) {
		tcols = ws.ws_col;
		tlines = ws.ws_row;
	} else
#endif
	{
		if ((tcols = tgetnum("co")) == -1)
			tcols = 80;
		if ((tlines = tgetnum("li")) == -1)
			tlines = 24;
	}
	tcols -= 4;
	tlines -= 4;
	TE = tgetstr("te", &tcp);
	TI = tgetstr("ti", &tcp);
	UP = tgetstr("up", &tcp);
	if (!(LL = tgetstr("ll", &tcp))) {
		if (!(LL = malloc((u_int)10))) {
			fprintf(stderr, "%s: out of space.\n", *argv);
			exit(1);
		}
		(void)strcpy(LL, tgoto(CM, 0, 23));
	}
	(void)signal(SIGHUP, onsig);
	(void)signal(SIGINT, onsig);
	(void)signal(SIGQUIT, onsig);
	(void)signal(SIGSTOP, onsig);
	(void)signal(SIGTSTP, onsig);
	(void)signal(SIGTERM, onsig);
	tcgetattr(1, &sg);
	old_tty = sg;
	ospeed = cfgetospeed(&sg);
	sg.c_iflag &= ~ICRNL;
	sg.c_oflag &= ~ONLCR;
	sg.c_lflag &= ~ECHO;
	tcsetattr(1, TCSADRAIN, &sg);
	if (TI)
		tputs(TI, 1, fputchar);
	tputs(tgetstr("cl", &tcp), 1, fputchar);
	tputs(tgetstr("vi", &tcp), 1, fputchar);                              
	(void)fflush(stdout);
	for (j = 4; j >= 0; --j) {
		xpos[j] = random() % tcols + 2;
		ypos[j] = random() % tlines + 2;
	}
	for (j = 0;;) {
		x = random() % tcols + 2;
		y = random() % tlines + 2;
		cursor(x, y);
		fputchar('.');
		cursor(xpos[j], ypos[j]);
		fputchar('o');
		if (!j--)
			j = 4;
		cursor(xpos[j], ypos[j]);
		fputchar('O');
		if (!j--)
			j = 4;
		cursor(xpos[j], ypos[j] - 1);
		fputchar('-');
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputs("|.|", stdout);
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputchar('-');
		if (!j--)
			j = 4;
		cursor(xpos[j], ypos[j] - 2);
		fputchar('-');
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputs("/ \\", stdout);
		cursor(xpos[j] - 2, ypos[j]);
		fputs("| O |", stdout);
		cursor(xpos[j] - 1, ypos[j] + 1);
		fputs("\\ /", stdout);
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputchar('-');
		if (!j--)
			j = 4;
		cursor(xpos[j], ypos[j] - 2);
		fputchar(' ');
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputchar(' ');
		tputs(ND, 1, fputchar);
		fputchar(' ');
		cursor(xpos[j] - 2, ypos[j]);
		fputchar(' ');
		tputs(ND, 1, fputchar);
		fputchar(' ');
		tputs(ND, 1, fputchar);
		fputchar(' ');
		cursor(xpos[j] - 1, ypos[j] + 1);
		fputchar(' ');
		tputs(ND, 1, fputchar);
		fputchar(' ');
		tputs(DN, 1, fputchar);
		tputs(BC, 1, fputchar);
		tputs(BC, 1, fputchar);
		fputchar(' ');
		xpos[j] = x;
		ypos[j] = y;
		(void)fflush(stdout);
		if (delay) usleep(delay);
	}
}

static void
onsig(dummy)
	int dummy;
{
	tputs(VE, 1, fputchar);                              
	tputs(LL, 1, fputchar);
	if (TE)
		tputs(TE, 1, fputchar);
	(void)fflush(stdout);
	tcsetattr(1, TCSADRAIN, &old_tty);
	exit(0);
}

int
fputchar(c)
	int c;
{
	return(putchar(c));
}
