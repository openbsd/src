
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
**	line drawing routines:
**	wborder()
**	whline()
**	wvline()
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_box.c,v 1.7 1997/04/12 17:51:49 tom Exp $")

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

int whline(WINDOW *win, chtype ch, int n)
{
short line;
short start;
short end;

	T((T_CALLED("whline(%p,%s,%d)"), win, _tracechtype(ch), n));

	line = win->_cury;
	start = win->_curx;
	end = start + n - 1;
	if (end > win->_maxx)
		end = win->_maxx;

	if (win->_line[line].firstchar == _NOCHANGE || win->_line[line].firstchar > start)
		win->_line[line].firstchar = start;
	if (win->_line[line].lastchar == _NOCHANGE || win->_line[line].lastchar < start)
		win->_line[line].lastchar = end;

	if (ch == 0)
		ch = ACS_HLINE;
	ch = _nc_render(win, ch);

	while ( end >= start) {
		win->_line[line].text[end] = ch;
		end--;
	}

	returnCode(OK);
}

int wvline(WINDOW *win, chtype ch, int n)
{
short row, col;
short end;

	T((T_CALLED("wvline(%p,%s,%d)"), win, _tracechtype(ch), n));

	row = win->_cury;
	col = win->_curx;
	end = row + n - 1;
	if (end > win->_maxy)
		end = win->_maxy;

	if (ch == 0)
		ch = ACS_VLINE;
	ch = _nc_render(win, ch);

	while(end >= row) {
		win->_line[end].text[col] = ch;
		if (win->_line[end].firstchar == _NOCHANGE || win->_line[end].firstchar > col)
			win->_line[end].firstchar = col;
		if (win->_line[end].lastchar == _NOCHANGE || win->_line[end].lastchar < col)
			win->_line[end].lastchar = col;
		end--;
	}

	_nc_synchook(win);
	returnCode(OK);
}

