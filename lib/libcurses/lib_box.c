
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

#include "curses.priv.h"

int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts,
	chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{
short i;
short endx, endy;

    T(("wborder() called"));

	if (ls == 0) ls = ACS_VLINE;
	if (rs == 0) rs = ACS_VLINE;
	if (ts == 0) ts = ACS_HLINE;
	if (bs == 0) bs = ACS_HLINE;
	if (tl == 0) tl = ACS_ULCORNER;
	if (tr == 0) tr = ACS_URCORNER;
	if (bl == 0) bl = ACS_LLCORNER;
	if (br == 0) br = ACS_LRCORNER;

	ls |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	rs |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	ts |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	bs |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	tl |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	tr |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	bl |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));
	br |= (win->_attrs ? win->_attrs : (win->_bkgd & A_ATTRIBUTES));

	T(("using %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx", ls, rs, ts, bs, tl, tr, bl, br));

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
	return OK;
}

int whline(WINDOW *win, chtype ch, int n)
{
short line;
short start;
short end;

	T(("whline(%p,%lx,%d) called", win, ch, n));

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
	while ( end >= start) {
		win->_line[line].text[end] = ch | win->_attrs;
		end--;
	}

	return OK;
}

int wvline(WINDOW *win, chtype ch, int n)
{
short row, col;
short end;

	T(("wvline(%p,%lx,%d) called", win, ch, n));

	row = win->_cury;
	col = win->_curx;
	end = row + n - 1;
	if (end > win->_maxy) 
		end = win->_maxy;

	if (ch == 0)
		ch = ACS_VLINE;

	while(end >= row) {
		win->_line[end].text[col] = ch | win->_attrs;
		if (win->_line[end].firstchar == _NOCHANGE || win->_line[end].firstchar > col) 
			win->_line[end].firstchar = col;
		if (win->_line[end].lastchar == _NOCHANGE || win->_line[end].lastchar < col) 
			win->_line[end].lastchar = col;
		end--;
	}

	_nc_synchook(win);
	return OK;
}
	
