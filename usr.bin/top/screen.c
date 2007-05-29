/* $OpenBSD: screen.c,v 1.17 2007/05/29 00:56:56 otto Exp $	 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains the routines that interface to termcap and stty/gtty.
 *
 * Paul Vixie, February 1987: converted to use ioctl() instead of stty/gtty.
 *
 * I put in code to turn on the TOSTOP bit while top was running, but I didn't
 * really like the results.  If you desire it, turn on the preprocessor
 * variable "TOStop".   --wnl
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <term.h>
#include <unistd.h>

#include "top.h"
#include "screen.h"
#include "boolean.h"

int	screen_length, screen_width;
char	ch_erase, ch_kill, smart_terminal;

static struct termios old_settings, new_settings;
static char is_a_terminal = No;

void
init_termcap(int interactive)
{
	char *term_name;
	int status;

	/* set defaults in case we aren't smart */
	screen_width = MAX_COLS;
	screen_length = 0;

	if (!interactive) {
		/* pretend we have a dumb terminal */
		smart_terminal = No;
		return;
	}
	/* assume we have a smart terminal until proven otherwise */
	smart_terminal = Yes;

	/* get the terminal name */
	term_name = getenv("TERM");

	/* if there is no TERM, assume it's a dumb terminal */
	/* patch courtesy of Sam Horrocks at telegraph.ics.uci.edu */
	if (term_name == NULL) {
		smart_terminal = No;
		return;
	}

	/* now get the termcap entry */
	if ((status = tgetent(NULL, term_name)) != 1) {
		if (status == -1)
			warnx("can't open termcap file");
		else
			warnx("no termcap entry for a `%s' terminal", term_name);

		/* pretend it's dumb and proceed */
		smart_terminal = No;
		return;
	}

	/* "hardcopy" immediately indicates a very stupid terminal */
	if (tgetflag("hc")) {
		smart_terminal = No;
		return;
	}

	/* set up common terminal capabilities */
	if ((screen_length = tgetnum("li")) <= Header_lines) {
		screen_length = smart_terminal = 0;
		return;
	}

	/* screen_width is a little different */
	if ((screen_width = tgetnum("co")) == -1)
		screen_width = 79;
	else
		screen_width -= 1;

        /* get necessary capabilities */
        if (tgetstr("cl", NULL) == NULL || tgetstr("cm", NULL) == NULL) {
                smart_terminal = No;
                return;
        }

	/* get the actual screen size with an ioctl, if needed */
	/*
	 * This may change screen_width and screen_length, and it always sets
	 * lower_left.
	 */
	get_screensize();

	/* if stdout is not a terminal, pretend we are a dumb terminal */
	if (tcgetattr(STDOUT_FILENO, &old_settings) == -1)
		smart_terminal = No;
}

void
init_screen(void)
{
	/* get the old settings for safe keeping */
	if (tcgetattr(STDOUT_FILENO, &old_settings) != -1) {
		/* copy the settings so we can modify them */
		new_settings = old_settings;
		/* turn off ICANON, character echo and tab expansion */
		new_settings.c_lflag &= ~(ICANON | ECHO);
		new_settings.c_oflag &= ~(OXTABS);
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;

		(void) tcsetattr(STDOUT_FILENO, TCSADRAIN, &new_settings);
		/* remember the erase and kill characters */
		ch_erase = old_settings.c_cc[VERASE];
		ch_kill = old_settings.c_cc[VKILL];

		is_a_terminal = Yes;
#if 0
		/* send the termcap initialization string */
		putcap(terminal_init);
#endif
	}
	if (!is_a_terminal) {
		/* not a terminal at all---consider it dumb */
		smart_terminal = No;
	}

	if (smart_terminal)
		initscr();
}

void
end_screen(void)
{
	if (smart_terminal)
		endwin();
	if (is_a_terminal)
		(void) tcsetattr(STDOUT_FILENO, TCSADRAIN, &old_settings);
}

void
reinit_screen(void)
{
#if 0
	/* install our settings if it is a terminal */
	if (is_a_terminal)
		(void) tcsetattr(STDOUT_FILENO, TCSADRAIN, &new_settings);

	/* send init string */
	if (smart_terminal)
		putcap(terminal_init);
#endif
}

void
get_screensize(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
		if (ws.ws_row != 0)
			screen_length = ws.ws_row;
		if (ws.ws_col != 0)
			screen_width = ws.ws_col - 1;
	}
}

void
go_home(void)
{
	if (smart_terminal) {
		move(0, 0);
		refresh();
	}
}

/* This has to be defined as a subroutine for tputs (instead of a macro) */
int
putstdout(int ch)
{
	int ret;

	ret = putchar(ch);
	if (ret == EOF)
		exit(1);
	return (ret);
}
