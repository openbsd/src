/*	$OpenBSD: main.c,v 1.7 1998/08/22 08:55:54 pjanzen Exp $	*/
/*	$NetBSD: main.c,v 1.5 1995/04/22 10:08:54 cgd Exp $	*/

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
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.7 1998/08/22 08:55:54 pjanzen Exp $";
#endif
#endif /* not lint */

#include	"robots.h"

int
main(ac, av)
	int	ac;
	char	**av;
{
	register char	*sp;
	register bool	bad_arg;
	register bool	show_only;
	extern char	*Scorefile;
	int		score_wfd;     /* high score writable file descriptor */
	int		score_err = 0; /* hold errno from score file open */

	if ((score_wfd = open(Scorefile, O_RDWR)) < 0)
		score_err = errno;

	/* revoke */
	setegid(getgid());
	setgid(getgid());

	show_only = FALSE;
	if (ac > 1) {
		bad_arg = FALSE;
		for (++av; ac > 1 && *av[0]; av++, ac--)
			if (av[0][0] != '-') {
				Scorefile = av[0];
				if (score_wfd >= 0)
					close(score_wfd);
			/* This file requires no special privileges. */
				if ((score_wfd = open(Scorefile, O_RDWR)) < 0)
					score_err = errno;
#ifdef	FANCY
				sp = strrchr(Scorefile, '/');
				if (sp == NULL)
					sp = Scorefile;
				if (strcmp(sp, "pattern_roll") == 0)
					Pattern_roll = TRUE;
				else if (strcmp(sp, "stand_still") == 0)
					Stand_still = TRUE;
				if (Pattern_roll || Stand_still)
					Teleport = TRUE;
#endif
			} else
				for (sp = &av[0][1]; *sp; sp++)
					switch (*sp) {
					  case 's':
						show_only = TRUE;
						break;
					  case 'r':
						Real_time = TRUE;
						break;
					  case 'a':
						Start_level = 4;
						break;
					  case 'j':
						Jump = TRUE;
						break;
					  case 't':
						Teleport = TRUE;
						break;
					  default:
						warnx("unknown option: %c", *sp);
						bad_arg = TRUE;
						break;
					}
		if (bad_arg) {
			exit(1);
			/* NOTREACHED */
		}
	}

	if (show_only) {
		show_score();
		exit(0);
	}

	if (score_wfd < 0)
		warnx("%s: %s; no scores will be saved", Scorefile,
			strerror(score_err));

	initscr();
	signal(SIGINT, quit);
	crmode();
	noecho();
	nonl();
	if (LINES != Y_SIZE || COLS != X_SIZE) {
		if (LINES < Y_SIZE || COLS < X_SIZE) {
			endwin();
			errx(1, "Need at least a %dx%d screen", Y_SIZE, X_SIZE);
		}
		delwin(stdscr);
		stdscr = newwin(Y_SIZE, X_SIZE, 0, 0);
	}

	srandom(getpid());
	if (Real_time)
		signal(SIGALRM, move_robots);
	do {
		init_field();
		for (Level = Start_level; !Dead; Level++) {
			make_level();
			play_level();
		}
		move(My_pos.y, My_pos.x);
		printw("AARRrrgghhhh....");
		refresh();
		score(score_wfd);
	} while (another());
	quit(0);
	/* NOT REACHED */
}

void
__cputchar(ch)
	int ch;
{
	(void)putchar(ch);
}

/*
 * quit:
 *	Leave the program elegantly.
 */
void
quit(dummy)
	int dummy;
{
	endwin();
	exit(0);
	/* NOTREACHED */
}

/*
 * another:
 *	See if another game is desired
 */
bool
another()
{
	register int	y;

#ifdef	FANCY
	if ((Stand_still || Pattern_roll) && !Newscore)
		return TRUE;
#endif

	if (query("Another game?")) {
		if (Full_clear) {
			for (y = 1; y <= Num_scores; y++) {
				move(y, 1);
				clrtoeol();
			}
			refresh();
		}
		return TRUE;
	}
	return FALSE;
}
