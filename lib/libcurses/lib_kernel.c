/*	$OpenBSD: lib_kernel.c,v 1.5 1998/07/23 21:18:58 millert Exp $	*/

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
 *	lib_kernel.c
 *
 *	Misc. low-level routines:
 *		reset_prog_mode()
 *		reset_shell_mode()
 *		erasechar()
 *		killchar()
 *		flushinp()
 *		savetty()
 *		resetty()
 *
 * The baudrate() and delay_output() functions could logically live here,
 * but are in other modules to reduce the static-link size of programs
 * that use only these facilities.
 */

#include <curses.priv.h>
#include <term.h>	/* cur_term */

MODULE_ID("$From: lib_kernel.c,v 1.17 1998/02/11 12:13:57 tom Exp $")

int reset_prog_mode(void)
{
	T((T_CALLED("reset_prog_mode()")));

	if (cur_term != 0) {
		_nc_set_curterm(&cur_term->Nttyb);
		if (SP && stdscr && stdscr->_use_keypad)
			_nc_keypad(TRUE);
		returnCode(OK);
	}
	returnCode(ERR);
}


int reset_shell_mode(void)
{
	T((T_CALLED("reset_shell_mode()")));

	if (cur_term != 0) {
		if (SP)
		{
			fflush(SP->_ofp);
			_nc_keypad(FALSE);
		}
		returnCode(_nc_set_curterm(&cur_term->Ottyb));
	}
	returnCode(ERR);
}

/*
 *	erasechar()
 *
 *	Return erase character as given in cur_term->Ottyb.
 *
 */

char
erasechar(void)
{
	T((T_CALLED("erasechar()")));

	if (cur_term != 0) {
#ifdef TERMIOS
		returnCode(cur_term->Ottyb.c_cc[VERASE]);
#else
		returnCode(cur_term->Ottyb.sg_erase);
#endif
	}
	returnCode(ERR);
}



/*
 *	killchar()
 *
 *	Return kill character as given in cur_term->Ottyb.
 *
 */

char
killchar(void)
{
	T((T_CALLED("killchar()")));

	if (cur_term != 0) {
#ifdef TERMIOS
		returnCode(cur_term->Ottyb.c_cc[VKILL]);
#else
		returnCode(cur_term->Ottyb.sg_kill);
#endif
	}
	returnCode(ERR);
}



/*
 *	flushinp()
 *
 *	Flush any input on cur_term->Filedes
 *
 */

int flushinp(void)
{
	T((T_CALLED("flushinp()")));

	if (cur_term != 0) {
#ifdef TERMIOS
		tcflush(cur_term->Filedes, TCIFLUSH);
#else
		errno = 0;
		do {
		    ioctl(cur_term->Filedes, TIOCFLUSH, 0);
		} while
		    (errno == EINTR);
#endif
		if (SP) {
			SP->_fifohead = -1;
			SP->_fifotail = 0;
			SP->_fifopeek = 0;
		}
		returnCode(OK);
	}
	returnCode(ERR);
}

/*
**	savetty()  and  resetty()
**
*/

static TTY   buf;

int savetty(void)
{
	T((T_CALLED("savetty()")));

	returnCode(_nc_get_curterm(&buf));
}

int resetty(void)
{
	T((T_CALLED("resetty()")));

	returnCode(_nc_set_curterm(&buf));
}
