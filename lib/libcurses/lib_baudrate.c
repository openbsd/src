/*	$OpenBSD: lib_baudrate.c,v 1.4 1998/07/23 21:18:27 millert Exp $	*/

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
 *	lib_baudrate.c
 *
 */

#include <curses.priv.h>
#include <term.h>	/* cur_term, pad_char */

MODULE_ID("$From: lib_baudrate.c,v 1.11 1998/02/11 12:13:58 tom Exp $")

/*
 *	int
 *	baudrate()
 *
 *	Returns the current terminal's baud rate.
 *
 */

struct speed {
	speed_t s;
	int sp;
};

static struct speed const speeds[] = {
	{B0, 0},
	{B50, 50},
	{B75, 75},
	{B110, 110},
	{B134, 134},
	{B150, 150},
	{B200, 200},
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B1800, 1800},
	{B2400, 2400},
	{B4800, 4800},
	{B9600, 9600},
#ifdef B19200
	{B19200, 19200},
#else
#ifdef EXTA
	{EXTA, 19200},
#endif
#endif
#ifdef B38400
	{B38400, 38400},
#else
#ifdef EXTB
	{EXTB, 38400},
#endif
#endif
#ifdef B57600
	{B57600, 57600},
#endif
#ifdef B115200
	{B115200, 115200},
#endif
#ifdef B230400
	{B230400, 230400},
#endif
#ifdef B460800
	{B460800, 460800},
#endif
};

int
baudrate(void)
{
size_t i;
int ret;
#ifdef TRACE
char *debug_rate;
#endif

	T((T_CALLED("baudrate()")));

	/*
	 * In debugging, allow the environment symbol to override when we're
	 * redirecting to a file, so we can construct repeatable test-cases
	 * that take into account costs that depend on baudrate.
	 */
#ifdef TRACE
	if (SP && !isatty(fileno(SP->_ofp))
	 && (debug_rate = getenv("BAUDRATE")) != 0) {
		long l;
		char *p;

		l = strtol(debug_rate, &p, 10);
		if (p == debug_rate || *p != '\0' || l == LONG_MIN ||
		    l > INT_MAX)
			l = 9600;
		returnCode((int)l);
	}
	else
#endif

#ifdef TERMIOS
	ret = cfgetospeed(&cur_term->Nttyb);
#else
	ret = cur_term->Nttyb.sg_ospeed;
#endif
	if(ret < 0 || (speed_t)ret > speeds[SIZEOF(speeds)-1].s)
		returnCode(ERR);
	cur_term->_baudrate = ERR;
	for (i = 0; i < SIZEOF(speeds); i++)
		if (speeds[i].s == (speed_t)ret)
		{
			cur_term->_baudrate = speeds[i].sp;
			break;
		}
	returnCode(cur_term->_baudrate);
}
