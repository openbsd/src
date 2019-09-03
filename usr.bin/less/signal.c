/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2015 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines dealing with signals.
 *
 * A signal usually merely causes a bit to be set in the "signals" word.
 * At some convenient time, the mainline code checks to see if any
 * signals need processing by calling psignal().
 */

#include <signal.h>

#include "less.h"

/*
 * signals which need to be processed.
 */
volatile sig_atomic_t signal_intr;
volatile sig_atomic_t signal_stop;
volatile sig_atomic_t signal_winch;

extern int sc_width, sc_height;
extern int screen_trashed;
extern int linenums;
extern int wscroll;
extern int quit_on_intr;
extern long jump_sline_fraction;

/*
 * Interrupt signal handler.
 */
static void
u_interrupt(int type)
{
	signal_intr = 1;
}

/*
 * "Stop" (^Z) signal handler.
 */
static void
stop(int type)
{
	signal_stop = 1;
}

/*
 * "Window" change handler
 */
void
sigwinch(int type)
{
	signal_winch = 1;
}

/*
 * Set up the signal handlers.
 */
void
init_signals(int on)
{
	if (on) {
		/*
		 * Set signal handlers.
		 */
		(void) lsignal(SIGINT, u_interrupt);
		(void) lsignal(SIGTSTP, stop);
		(void) lsignal(SIGWINCH, sigwinch);
		(void) lsignal(SIGQUIT, SIG_IGN);
	} else {
		/*
		 * Restore signals to defaults.
		 */
		(void) lsignal(SIGINT, SIG_DFL);
		(void) lsignal(SIGTSTP, SIG_DFL);
		(void) lsignal(SIGWINCH, SIG_IGN);
		(void) lsignal(SIGQUIT, SIG_DFL);
	}
}

/*
 * Process any signals we have received.
 */
void
psignals(void)
{
	if (signal_stop) {
		signal_stop = 0;
		/*
		 * Clean up the terminal.
		 */
		lsignal(SIGTTOU, SIG_IGN);
		clear_bot();
		deinit();
		flush(0);
		raw_mode(0);
		lsignal(SIGTTOU, SIG_DFL);
		lsignal(SIGTSTP, SIG_DFL);
		kill(getpid(), SIGTSTP);
		/*
		 * ... Bye bye. ...
		 * Hopefully we'll be back later and resume here...
		 * Reset the terminal and arrange to repaint the
		 * screen when we get back to the main command loop.
		 */
		lsignal(SIGTSTP, stop);
		raw_mode(1);
		init();
		screen_trashed = 1;
		signal_winch = 1;
	}
	if (signal_winch) {
		signal_winch = 0;
		int old_width, old_height;
		/*
		 * Re-execute scrsize() to read the new window size.
		 */
		old_width = sc_width;
		old_height = sc_height;
		get_term();
		if (sc_width != old_width || sc_height != old_height) {
			wscroll = (sc_height + 1) / 2;
			calc_jump_sline();
			calc_shift_count();
			screen_trashed = 1;
		}
	}
	if (signal_intr) {
		signal_intr = 0;
		ring_bell();
		if (quit_on_intr)
			quit(QUIT_INTERRUPT);
	}
}

/*
 * Custom version of signal() that causes syscalls to be interrupted.
 */
void *
lsignal(int s, void (*a)(int))
{
	struct sigaction sa, osa;

	sa.sa_handler = a;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;		/* don't restart system calls */
	if (sigaction(s, &sa, &osa) != 0)
		return (SIG_ERR);
	return (osa.sa_handler);
}

int
any_sigs(void)
{
	return (signal_intr || signal_stop || signal_winch);
}

int
abort_sigs(void)
{
	return (signal_intr || signal_stop);
}

