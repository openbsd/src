/*	$OpenBSD: lib_newterm.c,v 1.4 1998/01/17 16:27:34 millert Exp $	*/


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

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>	/* clear_screen, cup & friends, cur_term */

MODULE_ID("Id: lib_newterm.c,v 1.31 1997/12/28 00:36:51 tom Exp $")

#ifndef ONLCR		/* Allows compilation under the QNX 4.2 OS */
#define ONLCR 0
#endif

/*
 * SVr4/XSI Curses specify that hardware echo is turned off in initscr, and not
 * restored during the curses session.  The library simulates echo in software.
 * (The behavior is unspecified if the application enables hardware echo).
 *
 * The newterm function also initializes terminal settings, and since initscr
 * is supposed to behave as if it calls newterm, we do it here.
 */
static inline int _nc_initscr(void)
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
	return _nc_set_curterm(&cur_term->Nttyb);
}

/*
 * filter() has to be called before either initscr() or newterm(), so there is
 * apparently no way to make this flag apply to some terminals and not others,
 * aside from possibly delaying a filter() call until some terminals have been
 * initialized.
 */
static int filter_mode = FALSE;

void filter(void)
{
    filter_mode = TRUE;
}

SCREEN * newterm(const char *term, FILE *ofp, FILE *ifp)
{
int	errret;
SCREEN* current;
#ifdef TRACE
char *t = getenv("NCURSES_TRACE");

	if (t)
               trace((unsigned) strtol(t, 0, 0));
#endif

	T((T_CALLED("newterm(\"%s\",%p,%p)"), term, ofp, ifp));

	/* this loads the capability entry, then sets LINES and COLS */
	if (setupterm(term, fileno(ofp), &errret) == ERR)
		return 0;

	/*
	 * Check for mismatched graphic-rendition capabilities.  Most SVr4
	 * terminfo trees contain entries that have rmul or rmso equated to
	 * sgr0 (Solaris curses copes with those entries).  We do this only for
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
		clear_screen = 0;
		cursor_down = parm_down_cursor = 0;
		cursor_address = 0;
		cursor_up = parm_up_cursor = 0;
		row_address = 0;

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
		  return 0;
	      }
	/* this actually allocates the screen structure, and saves the
	 * original terminal settings.
	 */
	current = SP;
	_nc_set_screen(0);
	if (_nc_setupscreen(LINES, COLS, ofp) == ERR) {
	        _nc_set_screen(current);
		return 0;
	}

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

	/* Check whether we can optimize scrolling under dumb terminals in case
	 * we do not have any of these capabilities, scrolling optimization
	 * will be useless.
	 */
	SP->_scrolling = ((scroll_forward && scroll_reverse) ||
			  ((parm_rindex || parm_insert_line || insert_line) &&
			   (parm_index  || parm_delete_line || delete_line)));

	baudrate();	/* sets a field in the SP structure */

	/* compute movement costs so we can do better move optimization */
	_nc_mvcur_init();

	_nc_signal_handler(TRUE);

	/* initialize terminal to a sane state */
	_nc_screen_init();

	/* Initialize the terminal line settings. */
	_nc_initscr();

	T((T_RETURN("%p"), SP));
	return(SP);
}
