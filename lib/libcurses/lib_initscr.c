
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
**	lib_initscr.c
**
**	The routines initscr(), and termname().
**
*/

#include <curses.priv.h>
#include <term.h>	/* cur_term */

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>	/* needed for ISC */
#endif

MODULE_ID("Id: lib_initscr.c,v 1.18 1997/03/08 14:03:59 tom Exp $")

#ifndef ONLCR		/* Allows compilation under the QNX 4.2 OS */
#define ONLCR 0
#endif

/*
 * SVr4/XSI Curses specify that hardware echo is turned off in initscr, and not
 * restored during the curses session.  The library simulates echo in software.
 * (The behavior is unspecified if the application enables hardware echo).
 *
 * The newterm function also initializes terminal settings.
 */
int _nc_initscr(void)
{
	/* for extended XPG4 conformance requires cbreak() at this point */
	/* (SVr4 curses does this anyway) */
	cbreak();

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~(ECHO|ECHONL);
	cur_term->Nttyb.c_iflag &= ~(ICRNL|INLCR|IGNCR);
	cur_term->Nttyb.c_oflag &= ~(ONLCR);
#else
	cur_term->Nttyb.sg_flags &= ~(ECHO|CRMOD);
#endif
	if ((SET_TTY(cur_term->Filedes, &cur_term->Nttyb)) == -1)
		return ERR;
	return OK;
}

WINDOW *initscr(void)
{
static	bool initialized = FALSE;
const char *name;

	T((T_CALLED("initscr()")));
	/* Portable applications must not call initscr() more than once */
	if (!initialized) {
		initialized = TRUE;

		if ((name = getenv("TERM")) == 0)
			name = "unknown";
		if (newterm(name, stdout, stdin) == 0) {
			fprintf(stderr, "Error opening terminal: %s.\n", name);
			exit(EXIT_FAILURE);
		}

		/* allow user to set maximum escape delay from the environment */
		if ((name = getenv("ESCDELAY")) != 0)
			ESCDELAY = atoi(getenv("ESCDELAY"));

		/* def_shell_mode - done in newterm/_nc_setupscreen */
		def_prog_mode();
	}
	returnWin(stdscr);
}

char *termname(void)
{
char	*term = getenv("TERM");
static char	ret[15];

	T(("termname() called"));

	if (term != 0) {
		(void) strncpy(ret, term, sizeof(ret) - 1);
		ret[sizeof(ret) - 1] = '\0';
		term = ret;
	}
	return term;
}
