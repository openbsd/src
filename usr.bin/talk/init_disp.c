/*	$OpenBSD: init_disp.c,v 1.20 2009/10/27 23:59:44 deraadt Exp $	*/
/*	$NetBSD: init_disp.c,v 1.6 1994/12/09 02:14:17 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 * Initialization code for the display package,
 * as well as the signal handling routines.
 */

#include "talk.h"
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <err.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*
 * Set up curses, catch the appropriate signals,
 * and build the various windows.
 */
void
init_display(void)
{
	struct sigaction sa;

	if (initscr() == NULL)
		errx(1, "Terminal type unset or lacking necessary features.");
	(void) sigaction(SIGTSTP, NULL, &sa);
	sigaddset(&sa.sa_mask, SIGALRM);
	(void) sigaction(SIGTSTP, &sa, NULL);
	curses_initialized = 1;
	clear();
	refresh();
	noecho();
	cbreak();
	signal(SIGINT, sig_sent);
	signal(SIGPIPE, sig_sent);
	signal(SIGWINCH, sig_winch);
	/* curses takes care of ^Z */
	my_win.x_nlines = LINES / 2;
	my_win.x_ncols = COLS;
	my_win.x_win = newwin(my_win.x_nlines, my_win.x_ncols, 0, 0);
	scrollok(my_win.x_win, smooth_scroll);
	wclear(my_win.x_win);

	his_win.x_nlines = LINES / 2 - 1;
	his_win.x_ncols = COLS;
	his_win.x_win = newwin(his_win.x_nlines, his_win.x_ncols,
	    my_win.x_nlines+1, 0);
	scrollok(his_win.x_win, smooth_scroll);
	wclear(his_win.x_win);

	line_win = newwin(1, COLS, my_win.x_nlines, 0);
#if defined(NCURSES_VERSION) || defined(whline)
	whline(line_win, '-', COLS);
#else
	box(line_win, '-', '-');
#endif
	wrefresh(line_win);
	/* let them know we are working on it */
	current_state = "No connection yet";
}

/*
 * Trade edit characters with the other talk. By agreement
 * the first three characters each talk transmits after
 * connection are the three edit characters.
 */
void
set_edit_chars(void)
{
	u_char buf[3];
	int cc;
	struct termios tty;
	
	tcgetattr(STDIN_FILENO, &tty);
	buf[0] = my_win.cerase = (tty.c_cc[VERASE] == (u_char)_POSIX_VDISABLE)
	    ? CERASE : tty.c_cc[VERASE];
	buf[1] = my_win.kill = (tty.c_cc[VKILL] == (u_char)_POSIX_VDISABLE)
	    ? CKILL : tty.c_cc[VKILL];
	buf[2] = my_win.werase = (tty.c_cc[VWERASE] == (u_char)_POSIX_VDISABLE)
	    ? CWERASE : tty.c_cc[VWERASE];
	cc = write(sockt, buf, sizeof(buf));
	if (cc != sizeof(buf) )
		quit("Lost the connection", 1);
	cc = read(sockt, buf, sizeof(buf));
	if (cc != sizeof(buf) )
		quit("Lost the connection", 1);
	his_win.cerase = buf[0];
	his_win.kill = buf[1];
	his_win.werase = buf[2];
}

void
sig_sent(int dummy)
{

	quit("Connection closing.  Exiting", 0);
}

void
sig_winch(int dummy)
{

	gotwinch = 1;
}

/*
 * All done talking...hang up the phone and reset terminal thingy's
 */
void
quit(char *warning, int do_perror)
{

	if (curses_initialized) {
		wmove(his_win.x_win, his_win.x_nlines-1, 0);
		wclrtoeol(his_win.x_win);
		wrefresh(his_win.x_win);
		endwin();
	}
	if (invitation_waiting)
		send_delete();
	if (warning) {
		if (do_perror)
			warn("%s", warning);
		else
			warnx("%s", warning);
	}
	exit(0);
}

/*
 * If we get SIGWINCH, recompute both window sizes and refresh things.
 */
void
resize_display(void)
{
	struct winsize ws;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0 ||
	    (ws.ws_row == LINES && ws.ws_col == COLS))
		return;

	/* Update curses' internal state with new window size. */
	resizeterm(ws.ws_row, ws.ws_col);

	/*
	 * Resize each window but wait to refresh the screen until
	 * everything has been drawn so the cursor is in the right spot.
	 */
	my_win.x_nlines = LINES / 2;
	my_win.x_ncols = COLS;
	wresize(my_win.x_win, my_win.x_nlines, my_win.x_ncols);
	mvwin(my_win.x_win, 0, 0);
	clearok(my_win.x_win, TRUE);

	his_win.x_nlines = LINES / 2 - 1;
	his_win.x_ncols = COLS;
	wresize(his_win.x_win, his_win.x_nlines, his_win.x_ncols);
	mvwin(his_win.x_win, my_win.x_nlines + 1, 0);
	clearok(his_win.x_win, TRUE);

	wresize(line_win, 1, COLS);
	mvwin(line_win, my_win.x_nlines, 0);
#if defined(NCURSES_VERSION) || defined(whline)
	whline(line_win, '-', COLS);
#else
	wmove(line_win, my_win.x_nlines, 0);
	box(line_win, '-', '-');
#endif

	/* Now redraw the screen. */
	wrefresh(his_win.x_win);
	wrefresh(line_win);
	wrefresh(my_win.x_win);
}
