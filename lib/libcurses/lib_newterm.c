
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
**	lib_newterm.c
**
**	The newterm() function.
**
*/

#include <curses.priv.h>

#ifdef SVR4_TERMIO
#define _POSIX_SOURCE
#endif

#include <term.h>	/* clear_screen, cup & friends, cur_term */

MODULE_ID("Id: lib_newterm.c,v 1.23 1997/03/30 01:42:01 tom Exp $")

/* This should be moved to TERMINAL */
static int filter_mode = FALSE;

void filter(void)
{
    filter_mode = TRUE;
}

SCREEN * newterm(const char *term, FILE *ofp, FILE *ifp)
{
int	errret;
#ifdef TRACE
char *t = getenv("NCURSES_TRACE");

	if (t)
               trace((unsigned) strtol(t, 0, 0));
#endif

	T((T_CALLED("newterm(\"%s\",%p,%p)"), term, ofp, ifp));

	/* this loads the capability entry, then sets LINES and COLS */
	if (setupterm(term, fileno(ofp), &errret) == ERR)
		return NULL;

	/*
	 * Check for mismatched graphic-rendition capabilities.  Most SVr4
	 * terminfo tree contain entries that have rmul or rmso equated to sgr0
	 * (Solaris curses copes with those entries).  We do this only for
	 * curses, since many termcap applications assume that smso/rmso and
	 * smul/rmul are paired, and will not function properly if we remove
	 * rmso or rmul.  Curses applications shouldn't be looking at this
	 * detail.
	 */
	if (exit_attribute_mode) {
#define SGR0_FIX(mode) if (mode != 0 && !strcmp(mode, exit_attribute_mode)) \
			mode = 0
		SGR0_FIX(exit_underline_mode);
		SGR0_FIX(exit_standout_mode);
	}

	/* implement filter mode */
	if (filter_mode) {
		LINES = 1;

#ifdef init_tabs
		if (init_tabs != -1)
			TABSIZE = init_tabs;
		else
#endif /* init_tabs */
			TABSIZE = 8;

		T(("TABSIZE = %d", TABSIZE));

#ifdef clear_screen
		clear_screen = (char *)NULL;
		cursor_down = parm_down_cursor = (char *)NULL;
		cursor_address = (char *)NULL;
		cursor_up = parm_up_cursor = (char *)NULL;
		row_address = (char *)NULL;
		
		cursor_home = carriage_return;
#endif /* clear_screen */
	}

	/* If we must simulate soft labels, grab off the line to be used.
	   We assume that we must simulate, if it is none of the standard
	   formats (4-4  or 3-2-3) for which there may be some hardware
	   support. */
#ifdef num_labels
	if (num_labels <= 0 || !SLK_STDFMT)
#endif /* num_labels */
	    if (_nc_slk_format)
	      {
		if (ERR==_nc_ripoffline(-SLK_LINES, _nc_slk_initialize))
		  return NULL;
	      }
	/* this actually allocates the screen structure, and saves the
	 * original terminal settings.
	 */
	if (_nc_setupscreen(LINES, COLS, ofp) == ERR)
		return NULL;

#ifdef num_labels
	/* if the terminal type has real soft labels, set those up */
	if (_nc_slk_format && num_labels > 0 && SLK_STDFMT)
	    _nc_slk_initialize(stdscr, COLS);
#endif /* num_labels */

	SP->_ifd        = fileno(ifp);
	SP->_checkfd	= fileno(ifp);
	typeahead(fileno(ifp));
#ifdef TERMIOS
	SP->_use_meta   = ((cur_term->Ottyb.c_cflag & CSIZE) == CS8 &&
			    !(cur_term->Ottyb.c_iflag & ISTRIP));
#else
	SP->_use_meta   = FALSE;
#endif
	SP->_endwin	= FALSE;

	baudrate();	/* sets a field in the SP structure */

	/* compute movement costs so we can do better move optimization */
	_nc_mvcur_init();

	_nc_signal_handler(TRUE);

	/* open a connection to the screen's associated mouse, if any */
	_nc_mouse_init(SP);

	/* Initialize the terminal line settings. */
	_nc_initscr();

	T((T_RETURN("%p"), SP));
	return(SP);
}
