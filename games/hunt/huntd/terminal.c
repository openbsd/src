/*	$NetBSD: terminal.c,v 1.2 1997/10/10 16:34:05 lukem Exp $	*/
/*	$OpenBSD: terminal.c,v 1.2 1999/01/21 05:47:42 d Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
# include	"hunt.h"
# define	TERM_WIDTH	80	/* Assume terminals are 80-char wide */

/*
 * cgoto:
 *	Move the cursor to the given position on the given player's
 *	terminal.
 */
void
cgoto(pp, y, x)
	PLAYER	*pp;
	int	y, x;
{
	if (x == pp->p_curx && y == pp->p_cury)
		return;
	sendcom(pp, MOVE, y, x);
	pp->p_cury = y;
	pp->p_curx = x;
}

/*
 * outch:
 *	Put out a single character.
 */
void
outch(pp, ch)
	PLAYER	*pp;
	char	ch;
{
	if (++pp->p_curx >= TERM_WIDTH) {
		pp->p_curx = 0;
		pp->p_cury++;
	}
	(void) putc(ch, pp->p_output);
}

/*
 * outstr:
 *	Put out a string of the given length.
 */
void
outstr(pp, str, len)
	PLAYER	*pp;
	char	*str;
	int	len;
{
	pp->p_curx += len;
	pp->p_cury += (pp->p_curx / TERM_WIDTH);
	pp->p_curx %= TERM_WIDTH;
	while (len--)
		(void) putc(*str++, pp->p_output);
}

/*
 * clrscr:
 *	Clear the screen, and reset the current position on the screen.
 */
void
clrscr(pp)
	PLAYER	*pp;
{
	sendcom(pp, CLEAR);
	pp->p_cury = 0;
	pp->p_curx = 0;
}

/*
 * ce:
 *	Clear to the end of the line
 */
void
ce(pp)
	PLAYER	*pp;
{
	sendcom(pp, CLRTOEOL);
}

#if 0		/* XXX lukem*/
/*
 * ref;
 *	Refresh the screen
 */
void
ref(pp)
	PLAYER	*pp;
{
	sendcom(pp, REFRESH);
}
#endif

/*
 * sendcom:
 *	Send a command to the given user
 */
void
#if __STDC__
sendcom(PLAYER *pp, int command, ...)
#else
sendcom(pp, command, va_alist)
	PLAYER	*pp;
	int	command;
	va_dcl
#endif
{
	va_list	ap;
	int	arg1, arg2;
#if __STDC__
	va_start(ap, command);
#else
	va_start(ap);
#endif
	(void) putc(command, pp->p_output);
	switch (command & 0377) {
	case MOVE:
		arg1 = va_arg(ap, int);
		arg2 = va_arg(ap, int);
		(void) putc(arg1, pp->p_output);
		(void) putc(arg2, pp->p_output);
		break;
	case ADDCH:
	case READY:
		arg1 = va_arg(ap, int);
		(void) putc(arg1, pp->p_output);
		break;
	}

	va_end(ap);		/* No return needed for void functions. */
}
