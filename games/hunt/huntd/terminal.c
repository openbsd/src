/*	$OpenBSD: terminal.c,v 1.3 1999/01/29 07:30:37 d Exp $	*/
/*	$NetBSD: terminal.c,v 1.2 1997/10/10 16:34:05 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <stdarg.h>
#include "hunt.h"
#include "server.h"

#define	TERM_WIDTH	80	/* Assume terminals are 80-char wide */

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

	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			cgoto(pp, y, x);
		for (pp = Monitor; pp < End_monitor; pp++)
			cgoto(pp, y, x);
		return;
	}

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

	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			outch(pp, ch);
		for (pp = Monitor; pp < End_monitor; pp++)
			outch(pp, ch);
		return;
	}

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
	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			outstr(pp, str, len);
		for (pp = Monitor; pp < End_monitor; pp++)
			outstr(pp, str, len);
		return;
	}

	pp->p_curx += len;
	pp->p_cury += (pp->p_curx / TERM_WIDTH);
	pp->p_curx %= TERM_WIDTH;
	while (len--)
		(void) putc(*str++, pp->p_output);
}

/*
 * outat:
 *	draw a string at a location on the client.
 *	Cursor doesn't move if the location is invalid
 */
void
outyx(pp, y, x, fmt)
	PLAYER	*pp;
	int	y;
	int	x;
	const char *fmt;
{
	va_list ap;
	char buf[BUFSIZ];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof buf, fmt, ap);
	if (y >= 0 && x >= 0)
		cgoto(pp, y, x);
	outstr(pp, buf, len);
	va_end(ap);
}

/*
 * clrscr:
 *	Clear the screen, and reset the current position on the screen.
 */
void
clrscr(pp)
	PLAYER	*pp;
{

	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			clrscr(pp);
		for (pp = Monitor; pp < End_monitor; pp++)
			clrscr(pp);
		return;
	}

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

/*
 * sendcom:
 *	Send a command to the given user
 */
void
sendcom(pp, command)
	PLAYER *pp;
	int command;
{
	va_list	ap;
	char	buf[3];
	int	len = 0;

	va_start(ap, command);
	buf[len++] = command;
	switch (command & 0377) {
	case MOVE:
		buf[len++] = va_arg(ap, int);
		buf[len++] = va_arg(ap, int);
		break;
	case ADDCH:
	case READY:
	case ENDWIN:
		buf[len++] = va_arg(ap, int);
		break;
	}
	va_end(ap);

	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			fwrite(buf, sizeof buf[0], len, pp->p_output);
		for (pp = Monitor; pp < End_monitor; pp++)
			fwrite(buf, sizeof buf[0], len, pp->p_output);
		return;
	} else
		fwrite(buf, sizeof buf[0], len, pp->p_output);
}

/*
 * sync:
 *	Flush the output buffer to the player
 */
void
flush(pp)
	PLAYER	*pp;
{
	if (pp == ALL_PLAYERS) {
		for (pp = Player; pp < End_player; pp++)
			fflush(pp->p_output);
		for (pp = Monitor; pp < End_monitor; pp++)
			fflush(pp->p_output);
	} else
		fflush(pp->p_output);
}

