
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

#include "curses.priv.h"
#include <stdlib.h>
#include <string.h>

WINDOW *initscr(void)
{
char	*name = getenv("TERM");

	if (name == 0)
		name = "unknown";
  	if (newterm(name, stdout, stdin) == NULL) {
  		fprintf(stderr, "Error opening terminal: %s.\n", name);
  		exit(1);
	}

	/* allow user to set maximum escape delay from the environment */
	if ((name = getenv("ESCDELAY")))
	    ESCDELAY = atoi(getenv("ESCDELAY"));

	def_shell_mode();

	/* follow the XPG4 requirement to turn echo off at this point */
	noecho();

#ifdef _XOPEN_SOURCE_EXTENDED
	/* for extended XPG4 conformance requires cbreak() at this point */
	cbreak();
#endif /* _XOPEN_SOURCE_EXTENDED */

	def_prog_mode();
	return(stdscr);
}

char *termname(void)
{
char	*term = getenv("TERM");
static char	ret[15];

	T(("termname() called"));

	if (term == (char *)NULL)
		return(char *)NULL;
	else {
		(void) strncpy(ret, term, sizeof(ret) - 1);
		return(ret);
	}
}
