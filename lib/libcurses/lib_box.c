/*	$OpenBSD: lib_box.c,v 1.3 1997/12/03 05:21:12 millert Exp $	*/


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
**	lib_box.c
**
**	The routine wborder().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_box.c,v 1.9 1997/10/08 09:38:17 jtc Exp $")

int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts,
	chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{
short i;
short endx, endy;

    T((T_CALLED("wborder(%p,%s,%s,%s,%s,%s,%s,%s,%s)"),
	win,
	_tracechtype2(1,ls),
	_tracechtype2(2,rs),
	_tracechtype2(3,ts),
	_tracechtype2(4,bs),
	_tracechtype2(5,tl),
	_tracechtype2(6,tr),
	_tracechtype2(7,bl),
	_tracechtype2(8,br)));

        if (!win)
          returnCode(ERR);

	if (ls == 0) ls = ACS_VLINE;
	if (rs == 0) rs = ACS_VLINE;
	if (ts == 0) ts = ACS_HLINE;
	if (bs == 0) bs = ACS_HLINE;
	if (tl == 0) tl = ACS_ULCORNER;
	if (tr == 0) tr = ACS_URCORNER;
	if (bl == 0) bl = ACS_LLCORNER;
	if (br == 0) br = ACS_LRCORNER;

	ls = _nc_render(win, ls);
	rs = _nc_render(win, rs);
	ts = _nc_render(win, ts);
	bs = _nc_render(win, bs);
	tl = _nc_render(win, tl);
	tr = _nc_render(win, tr);
	bl = _nc_render(win, bl);
	br = _nc_render(win, br);

	T(("using %#lx, %#lx, %#lx, %#lx, %#lx, %#lx, %#lx, %#lx", ls, rs, ts, bs, tl, tr, bl, br));

	endx = win->_maxx;
	endy = win->_maxy;

	for (i = 0; i <= endx; i++) {
		win->_line[0].text[i] = ts;
		win->_line[endy].text[i] = bs;
	}
	win->_line[endy].firstchar = win->_line[0].firstchar = 0;
	win->_line[endy].lastchar = win->_line[0].lastchar = endx;

	for (i = 0; i <= endy; i++) {
		win->_line[i].text[0] =  ls;
		win->_line[i].text[endx] =  rs;
		win->_line[i].firstchar = 0;
		win->_line[i].lastchar = endx;
	}
	win->_line[0].text[0] = tl;
	win->_line[0].text[endx] = tr;
	win->_line[endy].text[0] = bl;
	win->_line[endy].text[endx] = br;

	_nc_synchook(win);
	returnCode(OK);
}
