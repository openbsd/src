/*	$OpenBSD: lib_ungetch.c,v 1.1 1997/12/03 05:21:39 millert Exp $	*/


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
**	lib_ungetch.c
**
**	The routine ungetch().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_ungetch.c,v 1.1 1997/10/19 02:59:56 tom Exp $")

#include <fifo_defs.h>

#ifdef TRACE
void _nc_fifo_dump(void)
{
int i;
	T(("head = %d, tail = %d, peek = %d", head, tail, peek));
	for (i = 0; i < 10; i++)
		T(("char %d = %s", i, _trace_key(SP->_fifo[i])));
}
#endif /* TRACE */

int ungetch(int ch)
{
	if (tail == -1)
		return ERR;
	if (head == -1) {
		head = 0;
		t_inc()
		peek = tail; /* no raw keys */
	} else
		h_dec();

	SP->_fifo[head] = ch;
	T(("ungetch %#x ok", ch));
#ifdef TRACE
	if (_nc_tracing & TRACE_IEVENT) _nc_fifo_dump();
#endif
	return OK;
}
