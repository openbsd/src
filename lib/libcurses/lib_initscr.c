/*	$OpenBSD: lib_initscr.c,v 1.3 1997/12/03 05:21:20 millert Exp $	*/


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

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>	/* needed for ISC */
#endif

MODULE_ID("Id: lib_initscr.c,v 1.19 1997/06/28 17:41:12 tom Exp $")

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
		term = ret;
	}
	return term;
}
