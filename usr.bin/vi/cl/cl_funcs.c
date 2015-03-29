/*	$OpenBSD: cl_funcs.c,v 1.18 2015/03/29 01:04:23 bcallah Exp $	*/

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "cl.h"

/*
 * cl_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int cl_addstr(SCR *, const char *, size_t);
 */
int
cl_addstr(SCR *sp, const char *str, size_t len)
{
	size_t oldy, oldx;
	int iv;

	/*
	 * If ex isn't in control, it's the last line of the screen and
	 * it's a split screen, use inverse video.
	 */
	iv = 0;
	getyx(stdscr, oldy, oldx);
	if (!F_ISSET(sp, SC_SCR_EXWROTE) &&
	    oldy == RLNO(sp, LASTLINE(sp)) && IS_SPLIT(sp)) {
		iv = 1;
		(void)standout();
	}

	if (addnstr(str, len) == ERR)
		return (1);

	if (iv)
		(void)standend();
	return (0);
}

/*
 * cl_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int cl_attr(SCR *, scr_attr_t, int);
 */
int
cl_attr(SCR *sp, scr_attr_t attribute, int on)
{
	CL_PRIVATE *clp;

	clp = CLP(sp);

	switch (attribute) {
	case SA_ALTERNATE:
	/*
	 * !!!
	 * There's a major layering violation here.  The problem is that the
	 * X11 xterm screen has what's known as an "alternate" screen.  Some
	 * xterm termcap/terminfo entries include sequences to switch to/from
	 * that alternate screen as part of the ti/te (smcup/rmcup) strings.
	 * Vi runs in the alternate screen, so that you are returned to the
	 * same screen contents on exit from vi that you had when you entered
	 * vi.  Further, when you run :shell, or :!date or similar ex commands,
	 * you also see the original screen contents.  This wasn't deliberate
	 * on vi's part, it's just that it historically sent terminal init/end
	 * sequences at those times, and the addition of the alternate screen
	 * sequences to the strings changed the behavior of vi.  The problem
	 * caused by this is that we don't want to switch back to the alternate
	 * screen while getting a new command from the user, when the user is
	 * continuing to enter ex commands, e.g.:
	 *
	 *	:!date				<<< switch to original screen
	 *	[Hit return to continue]	<<< prompt user to continue
	 *	:command			<<< get command from user
	 *
	 * Note that the :command input is a true vi input mode, e.g., input
	 * maps and abbreviations are being done.  So, we need to be able to
	 * switch back into the vi screen mode, without flashing the screen. 
	 *
	 * To make matters worse, the curses initscr() and endwin() calls will
	 * do this automatically -- so, this attribute isn't as controlled by
	 * the higher level screen as closely as one might like.
	 */
	if (on) {
		if (clp->ti_te != TI_SENT) {
			clp->ti_te = TI_SENT;
			if (clp->smcup == NULL)
				(void)cl_getcap(sp, "smcup", &clp->smcup);
			if (clp->smcup != NULL)
				(void)tputs(clp->smcup, 1, cl_putchar);
		}
	} else
		if (clp->ti_te != TE_SENT) {
			clp->ti_te = TE_SENT;
			if (clp->rmcup == NULL)
				(void)cl_getcap(sp, "rmcup", &clp->rmcup);
			if (clp->rmcup != NULL)
				(void)tputs(clp->rmcup, 1, cl_putchar);
			(void)fflush(stdout);
		}
		(void)fflush(stdout);
		break;
	case SA_INVERSE:
		if (F_ISSET(sp, SC_EX | SC_SCR_EXWROTE)) {
			if (clp->smso == NULL)
				return (1);
			if (on)
				(void)tputs(clp->smso, 1, cl_putchar);
			else
				(void)tputs(clp->rmso, 1, cl_putchar);
			(void)fflush(stdout);
		} else {
			if (on)
				(void)standout();
			else
				(void)standend();
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int cl_baud(SCR *, u_long *);
 */
int
cl_baud(SCR *sp, u_long *ratep)
{
	CL_PRIVATE *clp;

	/*
	 * XXX
	 * There's no portable way to get a "baud rate" -- cfgetospeed(3)
	 * returns the value associated with some #define, which we may
	 * never have heard of, or which may be a purely local speed.  Vi
	 * only cares if it's SLOW (w300), slow (w1200) or fast (w9600).
	 * Try and detect the slow ones, and default to fast.
	 */
	clp = CLP(sp);
	switch (cfgetospeed(&clp->orig)) {
	case B50:
	case B75:
	case B110:
	case B134:
	case B150:
	case B200:
	case B300:
	case B600:
		*ratep = 600;
		break;
	case B1200:
		*ratep = 1200;
		break;
	default:
		*ratep = 9600;
		break;
	}
	return (0);
}

/*
 * cl_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int cl_bell(SCR *);
 */
int
cl_bell(SCR *sp)
{
	if (F_ISSET(sp, SC_EX | SC_SCR_EXWROTE))
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
	else {
		/*
		 * If the screen has not been setup we cannot call
		 * curses routines yet.
		 */
		if (F_ISSET(sp, SC_SCR_VI)) {
			/*
			 * Vi has an edit option which determines if the
			 * terminal should be beeped or the screen flashed.
			 */
			if (O_ISSET(sp, O_FLASH))
				(void)flash();
			else
				(void)beep();
		} else if (!O_ISSET(sp, O_FLASH))
			(void)write(STDOUT_FILENO, "\07", 1);
	}
	return (0);
}

/*
 * cl_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int cl_clrtoeol(SCR *);
 */
int
cl_clrtoeol(SCR *sp)
{
	return (clrtoeol() == ERR);
}

/*
 * cl_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int cl_cursor(SCR *, size_t *, size_t *);
 */
int
cl_cursor(SCR *sp, size_t *yp, size_t *xp)
{
	/*
	 * The curses screen support splits a single underlying curses screen
	 * into multiple screens to support split screen semantics.  For this
	 * reason the returned value must be adjusted to be relative to the
	 * current screen, and not absolute.  Screens that implement the split
	 * using physically distinct screens won't need this hack.
	 */
	getyx(stdscr, *yp, *xp);
	*yp -= sp->woff;
	return (0);
}

/*
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int cl_deleteln(SCR *);
 */
int
cl_deleteln(SCR *sp)
{
	size_t oldy, oldx;

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 *
	 * Check for the existence of a chgat function; XSI requires it, but
	 * historic implementations of System V curses don't.   If it's not
	 * a #define, we'll fall back to doing it by hand, which is slow but
	 * acceptable.
	 *
	 * By hand means walking through the line, retrieving and rewriting
	 * each character.  Curses has no EOL marker, so track strings of
	 * spaces, and copy the trailing spaces only if there's a non-space
	 * character following.
	 */
	if (!F_ISSET(sp, SC_SCR_EXWROTE) && IS_SPLIT(sp)) {
		getyx(stdscr, oldy, oldx);
		mvchgat(RLNO(sp, LASTLINE(sp)), 0, -1, A_NORMAL, 0, NULL);
		(void)move(oldy, oldx);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and other screens must support that semantic.
	 */
	return (deleteln() == ERR);
}

/* 
 * cl_ex_adjust --
 *	Adjust the screen for ex.  This routine is purely for standalone
 *	ex programs.  All special purpose, all special case.
 *
 * PUBLIC: int cl_ex_adjust(SCR *, exadj_t);
 */
int
cl_ex_adjust(SCR *sp, exadj_t action)
{
	CL_PRIVATE *clp;
	int cnt;

	clp = CLP(sp);
	switch (action) {
	case EX_TERM_SCROLL:
		/* Move the cursor up one line if that's possible. */
		if (clp->cuu1 != NULL)
			(void)tputs(clp->cuu1, 1, cl_putchar);
		else if (clp->cup != NULL)
			(void)tputs(tgoto(clp->cup,
			    0, LINES - 2), 1, cl_putchar);
		else
			return (0);
		/* FALLTHROUGH */
	case EX_TERM_CE:
		/* Clear the line. */
		if (clp->el != NULL) {
			(void)putchar('\r');
			(void)tputs(clp->el, 1, cl_putchar);
		} else {
			/*
			 * Historically, ex didn't erase the line, so, if the
			 * displayed line was only a single glyph, and <eof>
			 * was more than one glyph, the output would not fully
			 * overwrite the user's input.  To fix this, output
			 * the maxiumum character number of spaces.  Note,
			 * this won't help if the user entered extra prompt
			 * or <blank> characters before the command character.
			 * We'd have to do a lot of work to make that work, and
			 * it's almost certainly not worth the effort.
			 */
			for (cnt = 0; cnt < MAX_CHARACTER_COLUMNS; ++cnt)
				(void)putchar('\b');
			for (cnt = 0; cnt < MAX_CHARACTER_COLUMNS; ++cnt)
				(void)putchar(' ');
			(void)putchar('\r');
			(void)fflush(stdout);
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int cl_insertln(SCR *);
 */
int
cl_insertln(SCR *sp)
{
	/*
	 * The current line is expected to be blank after this operation,
	 * and the screen must support that semantic.
	 */
	return (insertln() == ERR);
}

/*
 * cl_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int cl_keyval(SCR *, scr_keyval_t, CHAR_T *, int *);
 */
int
cl_keyval(SCR *sp, scr_keyval_t val, CHAR_T *chp, int *dnep)
{
	CL_PRIVATE *clp;

	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	clp = CLP(sp);
	switch (val) {
	case KEY_VEOF:
		*dnep = (*chp = clp->orig.c_cc[VEOF]) == _POSIX_VDISABLE;
		break;
	case KEY_VERASE:
		*dnep = (*chp = clp->orig.c_cc[VERASE]) == _POSIX_VDISABLE;
		break;
	case KEY_VKILL:
		*dnep = (*chp = clp->orig.c_cc[VKILL]) == _POSIX_VDISABLE;
		break;
	case KEY_VWERASE:
		*dnep = (*chp = clp->orig.c_cc[VWERASE]) == _POSIX_VDISABLE;
		break;
	default:
		*dnep = 1;
		break;
	}
	return (0);
}

/*
 * cl_move --
 *	Move the cursor.
 *
 * PUBLIC: int cl_move(SCR *, size_t, size_t);
 */
int
cl_move(SCR *sp, size_t lno, size_t cno)
{
	/* See the comment in cl_cursor. */
	if (move(RLNO(sp, lno), cno) == ERR) {
		msgq(sp, M_ERR,
		    "Error: move: l(%u) c(%u) o(%u)", lno, cno, sp->woff);
		return (1);
	}
	return (0);
}

/*
 * cl_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int cl_refresh(SCR *, int);
 */
int
cl_refresh(SCR *sp, int repaint)
{
	CL_PRIVATE *clp;

	clp = CLP(sp);

	/*
	 * If we received a killer signal, we're done, there's no point
	 * in refreshing the screen.
	 */
	if (clp->killersig)
		return (0);

	/*
	 * If repaint is set, the editor is telling us that we don't know
	 * what's on the screen, so we have to repaint from scratch.
	 *
	 * In the curses library, doing wrefresh(curscr) is okay, but the
	 * screen flashes when we then apply the refresh() to bring it up
	 * to date.  So, use clearok().
	 */
	if (repaint)
		clearok(curscr, 1);
	return (refresh() == ERR);
}

/*
 * cl_rename --
 *	Rename the file.
 *
 * PUBLIC: int cl_rename(SCR *, char *, int);
 */
int
cl_rename(SCR *sp, char *name, int on)
{
	GS *gp;
	CL_PRIVATE *clp;
	char *ttype;

	gp = sp->gp;
	clp = CLP(sp);

	ttype = OG_STR(gp, GO_TERM);

	/*
	 * XXX
	 * We can only rename windows for xterm.
	 */
	if (on) {
		if (F_ISSET(clp, CL_RENAME_OK) &&
		    !strncmp(ttype, "xterm", sizeof("xterm") - 1)) {
			F_SET(clp, CL_RENAME);
			(void)printf(XTERM_RENAME, name);
			(void)fflush(stdout);
		}
	} else
		if (F_ISSET(clp, CL_RENAME)) {
			F_CLR(clp, CL_RENAME);
			(void)printf(XTERM_RENAME, ttype);
			(void)fflush(stdout);
		}
	return (0);
}

/*
 * cl_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int cl_suspend(SCR *, int *);
 */
int
cl_suspend(SCR *sp, int *allowedp)
{
	struct termios t;
	CL_PRIVATE *clp;
	size_t oldy, oldx;
	int changed;

	clp = CLP(sp);
	*allowedp = 1;

	/*
	 * The ex implementation of this function isn't needed by screens not
	 * supporting ex commands that require full terminal canonical mode
	 * (e.g. :suspend).
	 *
	 * The vi implementation of this function isn't needed by screens not
	 * supporting vi process suspension, i.e. any screen that isn't backed
	 * by a UNIX shell.
	 *
	 * Setting allowedp to 0 will cause the editor to reject the command.
	 */
	if (F_ISSET(sp, SC_EX)) { 
		/* Save the terminal settings, and restore the original ones. */
		if (F_ISSET(clp, CL_STDIN_TTY)) {
			(void)tcgetattr(STDIN_FILENO, &t);
			(void)tcsetattr(STDIN_FILENO,
			    TCSASOFT | TCSADRAIN, &clp->orig);
		}

		/* Stop the process group. */
		(void)kill(0, SIGTSTP);

		/* Time passes ... */

		/* Restore terminal settings. */
		if (F_ISSET(clp, CL_STDIN_TTY))
			(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t);
		return (0);
	}

	/*
	 * Move to the lower left-hand corner of the screen.
	 *
	 * XXX
	 * Not sure this is necessary in System V implementations, but it
	 * shouldn't hurt.
	 */
	getyx(stdscr, oldy, oldx);
	(void)move(LINES - 1, 0);
	(void)refresh();

	/*
	 * Temporarily end the screen.  System V introduced a semantic where
	 * endwin() could be restarted.  We use it because restarting curses
	 * from scratch often fails in System V.
	 */

	/* Restore the cursor keys to normal mode. */
	(void)keypad(stdscr, FALSE);

	/* Restore the window name. */
	(void)cl_rename(sp, NULL, 0);

	(void)endwin();

	/*
	 * XXX
	 * Restore the original terminal settings.  This is bad -- the
	 * reset can cause character loss from the tty queue.  However,
	 * we can't call endwin() in BSD curses implementations, and too
	 * many System V curses implementations don't get it right.
	 */
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &clp->orig);

	/* Stop the process group. */
	(void)kill(0, SIGTSTP);

	/* Time passes ... */

	/*
	 * If we received a killer signal, we're done.  Leave everything
	 * unchanged.  In addition, the terminal has already been reset
	 * correctly, so leave it alone.
	 */
	if (clp->killersig) {
		F_CLR(clp, CL_SCR_EX_INIT | CL_SCR_VI_INIT);
		return (0);
	}

	/* Set the window name. */
	(void)cl_rename(sp, sp->frp->name, 1);

	/* Put the cursor keys into application mode. */
	(void)keypad(stdscr, TRUE);

	/* Refresh and repaint the screen. */
	(void)move(oldy, oldx);
	(void)cl_refresh(sp, 1);

	/* If the screen changed size, set the SIGWINCH bit. */
	if (cl_ssize(sp, 1, NULL, NULL, &changed))
		return (1);
	if (changed)
		F_SET(CLP(sp), CL_SIGWINCH);

	return (0);
}

/*
 * cl_usage --
 *	Print out the curses usage messages.
 * 
 * PUBLIC: void cl_usage(void);
 */
void
cl_usage()
{
	switch (pmode) {
	case MODE_EX:
		(void)fprintf(stderr, "usage: "
		    "ex [-FRrSsv] [-c cmd] [-t tag] [-w size] [file ...]\n");
		break;
	case MODE_VI:
		(void)fprintf(stderr, "usage: "
		    "vi [-eFRrS] [-c cmd] [-t tag] [-w size] [file ...]\n");
		break;
	case MODE_VIEW:
		(void)fprintf(stderr, "usage: "
		    "view [-eFrS] [-c cmd] [-t tag] [-w size] [file ...]\n");
		break;
	}
}

#ifdef DEBUG
/*
 * gdbrefresh --
 *	Stub routine so can flush out curses screen changes using gdb.
 */
int
gdbrefresh()
{
	refresh();
	return (0);		/* XXX Convince gdb to run it. */
}
#endif
