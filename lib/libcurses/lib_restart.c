/*	$OpenBSD: lib_restart.c,v 1.1 1998/07/23 21:19:18 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 * Terminfo-only terminal setup routines:
 *
 *		int restartterm(const char *, int, int *)
 *		TERMINAL *set_curterm(TERMINAL *)
 *		int del_curterm(TERMINAL *)
 */

#include <curses.priv.h>

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>	/* lines, columns, cur_term */

MODULE_ID("$From: lib_restart.c,v 1.16 1998/02/11 12:13:57 tom Exp $")

#undef tabs

#ifdef TAB3
# define tabs TAB3
#else
# ifdef XTABS
#  define tabs XTABS
# else
#  ifdef OXTABS
#   define tabs OXTABS
#  else
#   define tabs 0
#  endif
# endif
#endif

int def_shell_mode(void)
{
	T((T_CALLED("def_shell_mode()")));

	/*
	 * Turn off the XTABS bit in the tty structure if it was on.  If XTABS
	 * was on, remove the tab and backtab capabilities.
	 */

	if (_nc_get_curterm(&cur_term->Ottyb) != OK)
		returnCode(ERR);
#ifdef TERMIOS
	if (cur_term->Ottyb.c_oflag & tabs)
		tab = back_tab = NULL;
#else
	if (cur_term->Ottyb.sg_flags & XTABS)
		tab = back_tab = NULL;
#endif
	returnCode(OK);
}

int def_prog_mode(void)
{
	T((T_CALLED("def_prog_mode()")));

	if (_nc_get_curterm(&cur_term->Nttyb) != OK)
		returnCode(ERR);
#ifdef TERMIOS
	cur_term->Nttyb.c_oflag &= ~tabs;
#else
	cur_term->Nttyb.sg_flags &= ~XTABS;
#endif
	returnCode(OK);
}

int restartterm(const char *term, int filenum, int *errret)
{
int saveecho = SP->_echo;
int savecbreak = SP->_cbreak;
int saveraw = SP->_raw;
int savenl = SP->_nl;

	T((T_CALLED("restartterm(%s,%d,%p)"), term, filenum, errret));

	setupterm(term, filenum, errret);

	if (saveecho)
		echo();
	else
		noecho();

	if (savecbreak) {
		cbreak();
		noraw();
	} else if (saveraw) {
		nocbreak();
		raw();
	} else {
		nocbreak();
		noraw();
	}
	if (savenl)
		nl();
	else
		nonl();

	reset_prog_mode();

#if USE_SIZECHANGE
	_nc_update_screensize();
#endif

	returnCode(OK);
}
