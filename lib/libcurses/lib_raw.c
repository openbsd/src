/*	$OpenBSD: lib_raw.c,v 1.5 1998/07/23 21:19:14 millert Exp $	*/

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
 *	raw.c
 *
 *	Routines:
 *		raw()
 *		cbreak()
 *		noraw()
 *		nocbreak()
 *		qiflush()
 *		noqiflush()
 *		intrflush()
 *
 */

#include <curses.priv.h>
#include <term.h>	/* cur_term */

MODULE_ID("$From: lib_raw.c,v 1.26 1998/04/11 23:00:07 tom Exp $")

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>	/* needed for ISC */
#endif

#ifdef __EMX__
#include <io.h>
#include <fcntl.h>
#endif

/* may be undefined if we're using termio.h */
#ifndef TOSTOP
#define TOSTOP 0
#endif
#ifndef IEXTEN
#define IEXTEN 0
#endif

#define COOKED_INPUT	(IXON|BRKINT|PARMRK)

#ifdef TRACE

typedef struct {unsigned int val; const char *name;} BITNAMES;

static void lookup_bits(char *buf, const BITNAMES *table, const char *label, unsigned int val)
{
	const BITNAMES *sp;

	(void) strcat(buf, label);
	(void) strcat(buf, ": {");
	for (sp = table; sp->name; sp++)
		if (sp->val != 0
		&& (val & sp->val) == sp->val)
		{
			(void) strcat(buf, sp->name);
			(void) strcat(buf, ", ");
		}
	if (buf[strlen(buf) - 2] == ',')
		buf[strlen(buf) - 2] = '\0';
	(void) strcat(buf,"} ");
}

char *_tracebits(void)
/* describe the state of the terminal control bits exactly */
{
char	*buf;
static const	BITNAMES

#ifdef TERMIOS
iflags[] =
    {
	{BRKINT,	"BRKINT"},
	{IGNBRK,	"IGNBRK"},
	{IGNPAR,	"IGNPAR"},
	{PARMRK,	"PARMRK"},
	{INPCK, 	"INPCK"},
	{ISTRIP,	"ISTRIP"},
	{INLCR, 	"INLCR"},
	{IGNCR, 	"IGNC"},
	{ICRNL, 	"ICRNL"},
	{IXON,  	"IXON"},
	{IXOFF, 	"IXOFF"},
	{0,		NULL}
#define ALLIN	(BRKINT|IGNBRK|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF)
    },
oflags[] =
    {
	{OPOST, 	"OPOST"},
	{0,		NULL}
#define ALLOUT	(OPOST)
    },
cflags[] =
    {
	{CLOCAL,	"CLOCAL"},
	{CREAD, 	"CREAD"},
	{CSTOPB,	"CSTOPB"},
#if !defined(CS5) || !defined(CS8)
	{CSIZE, 	"CSIZE"},
#endif
	{HUPCL, 	"HUPCL"},
	{PARENB,	"PARENB"},
	{PARODD|PARENB,	"PARODD"},	/* concession to readability */
	{0,		NULL}
#define ALLCTRL	(CLOCAL|CREAD|CSIZE|CSTOPB|HUPCL|PARENB|PARODD)
    },
lflags[] =
    {
	{ECHO,  	"ECHO"},
	{ECHOE|ECHO, 	"ECHOE"},	/* concession to readability */
	{ECHOK|ECHO, 	"ECHOK"},	/* concession to readability */
	{ECHONL,	"ECHONL"},
	{ICANON,	"ICANON"},
	{ISIG,  	"ISIG"},
	{NOFLSH,	"NOFLSH"},
	{TOSTOP,	"TOSTOP"},
	{IEXTEN,	"IEXTEN"},
	{0,		NULL}
#define ALLLOCAL	(ECHO|ECHONL|ICANON|ISIG|NOFLSH|TOSTOP|IEXTEN)
    };


    buf = _nc_trace_buf(0,
    	8 + sizeof(iflags) +
    	8 + sizeof(oflags) +
    	8 + sizeof(cflags) +
    	8 + sizeof(lflags) +
	8);

    if (cur_term->Nttyb.c_iflag & ALLIN)
	lookup_bits(buf, iflags, "iflags", cur_term->Nttyb.c_iflag);

    if (cur_term->Nttyb.c_oflag & ALLOUT)
	lookup_bits(buf, oflags, "oflags", cur_term->Nttyb.c_oflag);

    if (cur_term->Nttyb.c_cflag & ALLCTRL)
	lookup_bits(buf, cflags, "cflags", cur_term->Nttyb.c_cflag);

#if defined(CS5) && defined(CS8)
    switch (cur_term->Nttyb.c_cflag & CSIZE) {
    case CS5:	strcat(buf, "CS5 ");	break;
    case CS6:	strcat(buf, "CS6 ");	break;
    case CS7:	strcat(buf, "CS7 ");	break;
    case CS8:	strcat(buf, "CS8 ");	break;
    default:	strcat(buf, "CSIZE? ");	break;
    }
#endif

    if (cur_term->Nttyb.c_lflag & ALLLOCAL)
	lookup_bits(buf, lflags, "lflags", cur_term->Nttyb.c_lflag);

#else
    /* reference: ttcompat(4M) on SunOS 4.1 */
#ifndef EVENP
#define EVENP 0
#endif
#ifndef LCASE
#define LCASE 0
#endif
#ifndef LLITOUT
#define LLITOUT 0
#endif
#ifndef ODDP
#define ODDP 0
#endif
#ifndef TANDEM
#define TANDEM 0
#endif

cflags[] =
    {
	{CBREAK,	"CBREAK"},
	{CRMOD,		"CRMOD"},
	{ECHO,		"ECHO"},
	{EVENP,		"EVENP"},
	{LCASE,		"LCASE"},
	{LLITOUT,	"LLITOUT"},
	{ODDP,		"ODDP"},
	{RAW,		"RAW"},
	{TANDEM,	"TANDEM"},
	{XTABS,		"XTABS"},
	{0,		NULL}
#define ALLCTRL	(CBREAK|CRMOD|ECHO|EVENP|LCASE|LLITOUT|ODDP|RAW|TANDEM|XTABS)
    };

    buf = _nc_trace_buf(0,
    	8 + sizeof(cflags));

    if (cur_term->Nttyb.sg_flags & ALLCTRL)
    {
	lookup_bits(buf, cflags, "cflags", cur_term->Nttyb.sg_flags);
    }

#endif
    return(buf);
}

#define BEFORE(N)	if (_nc_tracing&TRACE_BITS) _tracef("%s before bits: %s", N, _tracebits())
#define AFTER(N)	if (_nc_tracing&TRACE_BITS) _tracef("%s after bits: %s", N, _tracebits())
#else
#define BEFORE(s)
#define AFTER(s)
#endif /* TRACE */

int raw(void)
{
	T((T_CALLED("raw()")));
	if (SP != 0 && cur_term != 0) {

		SP->_raw = TRUE;
		SP->_cbreak = TRUE;

#ifdef __EMX__
		setmode(SP->_ifd, O_BINARY);
#endif

#ifdef TERMIOS
		BEFORE("raw");
		cur_term->Nttyb.c_lflag &= ~(ICANON|ISIG);
		cur_term->Nttyb.c_iflag &= ~(COOKED_INPUT);
		cur_term->Nttyb.c_cc[VMIN] = 1;
		cur_term->Nttyb.c_cc[VTIME] = 0;
		AFTER("raw");
#else
		cur_term->Nttyb.sg_flags |= RAW;
#endif
		returnCode(_nc_set_curterm(&cur_term->Nttyb));
	}
	returnCode(ERR);
}

int cbreak(void)
{
	T((T_CALLED("cbreak()")));

	SP->_cbreak = TRUE;

#ifdef __EMX__
	setmode(SP->_ifd, O_BINARY);
#endif

#ifdef TERMIOS
	BEFORE("cbreak");
	cur_term->Nttyb.c_lflag &= ~ICANON;
	cur_term->Nttyb.c_iflag &= ~ICRNL;
	cur_term->Nttyb.c_lflag |= ISIG;
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	AFTER("cbreak");
#else
	cur_term->Nttyb.sg_flags |= CBREAK;
#endif
	returnCode(_nc_set_curterm( &cur_term->Nttyb));
}

void qiflush(void)
{
	T((T_CALLED("qiflush()")));

	/*
	 * Note: this implementation may be wrong.  See the comment under
	 * intrflush().
	 */

#ifdef TERMIOS
	BEFORE("qiflush");
	cur_term->Nttyb.c_lflag &= ~(NOFLSH);
	AFTER("qiflush");
	(void)_nc_set_curterm( &cur_term->Nttyb);
	returnVoid;
#endif
}


int noraw(void)
{
	T((T_CALLED("noraw()")));

	SP->_raw = FALSE;
	SP->_cbreak = FALSE;

#ifdef __EMX__
	setmode(SP->_ifd, O_TEXT);
#endif

#ifdef TERMIOS
	BEFORE("noraw");
	cur_term->Nttyb.c_lflag |= ISIG|ICANON;
	cur_term->Nttyb.c_iflag |= COOKED_INPUT;
	AFTER("noraw");
#else
	cur_term->Nttyb.sg_flags &= ~(RAW|CBREAK);
#endif
	returnCode(_nc_set_curterm( &cur_term->Nttyb));
}


int nocbreak(void)
{
	T((T_CALLED("nocbreak()")));

	SP->_cbreak = FALSE;

#ifdef __EMX__
	setmode(SP->_ifd, O_TEXT);
#endif

#ifdef TERMIOS
	BEFORE("nocbreak");
	cur_term->Nttyb.c_lflag |= ICANON;
	cur_term->Nttyb.c_iflag |= ICRNL;
	AFTER("nocbreak");
#else
	cur_term->Nttyb.sg_flags &= ~CBREAK;
#endif
	returnCode(_nc_set_curterm( &cur_term->Nttyb));
}

void noqiflush(void)
{
	T((T_CALLED("noqiflush()")));

	/*
	 * Note: this implementation may be wrong.  See the comment under
	 * intrflush().
	 */

#ifdef TERMIOS
	BEFORE("noqiflush");
	cur_term->Nttyb.c_lflag |= NOFLSH;
	AFTER("noqiflush");
	(void)_nc_set_curterm( &cur_term->Nttyb);
	returnVoid;
#endif
}

int intrflush(WINDOW *win GCC_UNUSED, bool flag)
{
	T((T_CALLED("intrflush(%d)"), flag));

	/*
	 * This call does the same thing as the qiflush()/noqiflush()
	 * pair.  We know for certain that SVr3 intrflush() tweaks the
	 * NOFLSH bit; on the other hand, the match (in the SVr4 man
	 * pages) between the language describing NOFLSH in termio(7)
	 * and the language describing qiflush()/noqiflush() in
	 * curs_inopts(3x) is too exact to be coincidence.
	 */

#ifdef TERMIOS
	BEFORE("intrflush");
	if (flag)
		cur_term->Nttyb.c_lflag &= ~(NOFLSH);
	else
		cur_term->Nttyb.c_lflag |= (NOFLSH);
	AFTER("intrflush");
	returnCode(_nc_set_curterm( &cur_term->Nttyb));
#else
	returnCode(ERR);
#endif
}
