/*	$OpenBSD: lib_termcap.c,v 1.1 1998/07/23 21:19:37 millert Exp $	*/

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

#include <curses.priv.h>

#include <termcap.h>
#include <tic.h>

#define __INTERNAL_CAPS_VISIBLE
#include <term.h>

MODULE_ID("$From: lib_termcap.c,v 1.20 1998/07/18 02:15:40 tom Exp $")

/*
   some of the code in here was contributed by:
   Magnus Bengtsson, d6mbeng@dtek.chalmers.se
*/

char PC;
char *UP;
char *BC;
speed_t ospeed;

/***************************************************************************
 *
 * tgetent(bufp, term)
 *
 * In termcap, this function reads in the entry for terminal `term' into the
 * buffer pointed to by bufp. It must be called before any of the functions
 * below are called.
 * In this terminfo emulation, tgetent() simply calls setupterm() (which
 * does a bit more than tgetent() in termcap does), and returns its return
 * value (1 if successful, 0 if no terminal with the given name could be
 * found, or -1 if no terminal descriptions have been installed on the
 * system).  The bufp argument is ignored.
 *
 ***************************************************************************/

int tgetent(char *bufp GCC_UNUSED, const char *name)
{
int errcode;
#if defined(TERMIOS)
speed_t speed;
#endif

	T(("calling tgetent"));
	setupterm(name, STDOUT_FILENO, &errcode);

	if (errcode != 1)
		return(errcode);

	if (cursor_left)
	    if ((backspaces_with_bs = !strcmp(cursor_left, "\b")) == 0)
		backspace_if_not_bs = cursor_left;

	/* we're required to export these */
	if (pad_char != NULL)
		PC = pad_char[0];
	if (cursor_up != NULL)
		UP = cursor_up;
	if (backspace_if_not_bs != NULL)
		BC = backspace_if_not_bs;
#if defined(TERMIOS)
	/*
	 * Back-convert to the funny speed encoding used by the old BSD
	 * curses library.  Method suggested by Andrey Chernov
	 * <ache@astral.msk.su>
	 */
	if ((speed = cfgetospeed(&cur_term->Nttyb)) < 1)
	    ospeed = 1;		/* assume lowest non-hangup speed */
	else
	{
		const speed_t *sp;
		static const speed_t speeds[] = {
#ifdef B115200
			B115200,
#endif
#ifdef B57600
			B57600,
#endif
#ifdef B38400
			B38400,
#else
#ifdef EXTB
			EXTB,
#endif
#endif /* B38400 */
#ifdef B19200
			B19200,
#else
#ifdef EXTA
			EXTA,
#endif
#endif /* B19200 */
			B9600,
			B4800,
			B2400,
			B1800,
			B1200,
			B600,
			B300,
			B200,
			B150,
			B134,
			B110,
			B75,
			B50,
			B0,
		};
#define MAXSPEED	SIZEOF(speeds)

		for (sp = speeds; sp < speeds + MAXSPEED; sp++) {
			if (sp[0] <= speed) {
				break;
			}
		}
		ospeed = MAXSPEED - (sp - speeds);
	}
#else
	ospeed = cur_term->Nttyb.sg_ospeed;
#endif

/* LINT_PREPRO
#if 0*/
#include <capdefaults.c>
/* LINT_PREPRO
#endif*/

	return errcode;
}

/***************************************************************************
 *
 * tgetflag(str)
 *
 * Look up boolean termcap capability str and return its value (TRUE=1 if
 * present, FALSE=0 if not).
 *
 ***************************************************************************/

int tgetflag(const char *id)
{
int i;

	T(("tgetflag: %s", id));
	if (cur_term != 0) {
		for (i = 0; i < BOOLCOUNT; i++) {
			if (!strcmp(id, boolcodes[i])) {
				if (!VALID_BOOLEAN(cur_term->type.Booleans[i]))
					return 0;
				return cur_term->type.Booleans[i];
			}
		}
	}
	return 0;	/* Solaris does this */
}

/***************************************************************************
 *
 * tgetnum(str)
 *
 * Look up numeric termcap capability str and return its value, or -1 if
 * not given.
 *
 ***************************************************************************/

int tgetnum(const char *id)
{
int i;

	T(("tgetnum: %s", id));
	if (cur_term != 0) {
		for (i = 0; i < NUMCOUNT; i++) {
			if (!strcmp(id, numcodes[i])) {
				if (!VALID_NUMERIC(cur_term->type.Numbers[i]))
					return -1;
				return cur_term->type.Numbers[i];
			}
		}
	}
	return ERR;
}

/***************************************************************************
 *
 * tgetstr(str, area)
 *
 * Look up string termcap capability str and return a pointer to its value,
 * or NULL if not given.
 *
 ***************************************************************************/

char *tgetstr(const char *id, char **area GCC_UNUSED)
{
int i;

	T(("tgetstr: %s", id));
	if (cur_term != 0) {
		for (i = 0; i < STRCOUNT; i++) {
			T(("trying %s", strcodes[i]));
			if (!strcmp(id, strcodes[i])) {
				T(("found match : %s", _nc_visbuf(cur_term->type.Strings[i])));
				if (!VALID_STRING(cur_term->type.Strings[i]))
					return 0;
				return cur_term->type.Strings[i];
			}
		}
	}
	return NULL;
}

/*
 *	char *
 *	tgoto(string, x, y)
 *
 *	Retained solely for upward compatibility.  Note the intentional
 *	reversing of the last two arguments.
 *
 */

char *tgoto(const char *string, int x, int y)
{
	return(tparm((NCURSES_CONST char *)string, y, x));
}
