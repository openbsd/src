
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
**	lib_window.c
**
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_window.c,v 1.8 1997/02/02 01:14:43 tom Exp $")

void _nc_synchook(WINDOW *win)
/* hook to be called after each window change */
{
	if (win->_immed) wrefresh(win);
	if (win->_sync) wsyncup(win);
}

int mvderwin(WINDOW *win, int y, int x)
/* move a derived window */
{
   WINDOW *orig = win->_parent;
   int i;

   T((T_CALLED("mvderwin(%p,%d,%d)"), win, y, x));

   if (orig)
   {
      if (win->_parx==x && win->_pary==y)
	returnCode(OK);
      if (x<0 || y<0)
	returnCode(ERR);
      if ( (x+getmaxx(win) > getmaxx(orig)) ||
           (y+getmaxy(win) > getmaxy(orig)) )
        returnCode(ERR);
   }
   else
      returnCode(ERR);
   wsyncup(win);
   win->_parx = x;
   win->_pary = y;
   for(i=0;i<getmaxy(win);i++)
     win->_line[i].text = &(orig->_line[y++].text[x]);
   returnCode(OK);
}

int syncok(WINDOW *win, bool bf)
/* enable/disable automatic wsyncup() on each change to window */
{
	T((T_CALLED("syncok(%p,%d)"), win, bf));

	if (win) {
		win->_sync = bf;
		returnCode(OK);
	} else
		returnCode(ERR);
}

void wsyncup(WINDOW *win)
/* mark changed every cell in win's ancestors that is changed in win */
/* Rewritten by J. Pfeifer, 1-Apr-96 (don't even think that...)      */
{
  WINDOW	*wp;

  if (win && win->_parent)
    for (wp = win; wp->_parent; wp = wp->_parent)
      {
	int y;
	WINDOW *pp = wp->_parent;

	assert((wp->_pary <= pp->_maxy) &&
	       ((wp->_pary+wp->_maxy) <= pp->_maxy));

	for (y = 0; y <= wp->_maxy; y++)
	  {
	    int left = wp->_line[y].firstchar;
	    if (left >= 0) /* line is touched */
	      {
		/* left & right character in parent window coordinates */
		int right = wp->_line[y].lastchar + wp->_parx;
		left += wp->_parx;

		if (pp->_line[wp->_pary + y].firstchar == _NOCHANGE)
		  {
		    pp->_line[wp->_pary + y].firstchar = left;
		    pp->_line[wp->_pary + y].lastchar  = right;
		  }
		else
		  {
		    if (left < pp->_line[wp->_pary + y].firstchar)
		      pp->_line[wp->_pary + y].firstchar = left;
		    if (pp->_line[wp->_pary + y].lastchar < right)
		      pp->_line[wp->_pary + y].lastchar = right;
		  }
	      }
	  }
      }
}

void wsyncdown(WINDOW *win)
/* mark changed every cell in win that is changed in any of its ancestors */
/* Rewritten by J. Pfeifer, 1-Apr-96 (don't even think that...)           */
{
  if (win && win->_parent)
    {
      WINDOW *pp = win->_parent;
      int y;

      /* This recursion guarantees, that the changes are propagated down-
	 wards from the root to our direct parent. */
      wsyncdown(pp);

      /* and now we only have to propagate the changes from our direct
	 parent, if there are any. */
      assert((win->_pary <= pp->_maxy) &&
	     ((win->_pary + win->_maxy) <= pp->_maxy));

      for (y = 0; y <= win->_maxy; y++)
	{
	  if (pp->_line[win->_pary + y].firstchar >= 0) /* parent changed */
	    {
	      /* left and right character in child coordinates */
	      int left  = pp->_line[win->_pary + y].firstchar - win->_parx;
	      int right = pp->_line[win->_pary + y].lastchar  - win->_parx;
	      /* The change maybe outside the childs range */
	      if (left<0)
		left = 0;
	      if (right > win->_maxx)
		right = win->_maxx;
	      if (win->_line[y].firstchar == _NOCHANGE)
		{
		  win->_line[y].firstchar = left;
		  win->_line[y].lastchar  = right;
		}
	      else
		{
		  if (left < win->_line[y].firstchar)
		    win->_line[y].firstchar = left;
		  if (win->_line[y].lastchar < right)
		    win->_line[y].lastchar = right;
		}
	    }
	}
    }
}

void wcursyncup(WINDOW *win)
/* sync the cursor in all derived windows to its value in the base window */
{
   WINDOW *wp;
   for( wp = win; wp && wp->_parent; wp = wp->_parent ) {
      wmove( wp->_parent, wp->_pary + wp->_cury, wp->_parx + wp->_curx );
   }
}

WINDOW *dupwin(WINDOW *win)
/* make an exact duplicate of the given window */
{
WINDOW *nwin;
size_t linesize;
int i;

	T((T_CALLED("dupwin(%p)"), win));

	if ((nwin = newwin(win->_maxy + 1, win->_maxx + 1, win->_begy, win->_begx)) == NULL)
		returnWin(0);

	nwin->_curx        = win->_curx;
	nwin->_cury        = win->_cury;
	nwin->_maxy        = win->_maxy;
	nwin->_maxx        = win->_maxx;
	nwin->_begy        = win->_begy;
	nwin->_begx        = win->_begx;
	nwin->_yoffset     = win->_yoffset;

	nwin->_flags       = win->_flags;
	nwin->_attrs       = win->_attrs;
	nwin->_bkgd        = win->_bkgd;

	nwin->_clear       = win->_clear;
	nwin->_scroll      = win->_scroll;
	nwin->_leaveok     = win->_leaveok;
	nwin->_use_keypad  = win->_use_keypad;
	nwin->_delay       = win->_delay;
	nwin->_immed       = win->_immed;
	nwin->_sync        = win->_sync;
	nwin->_parx        = win->_parx;
	nwin->_pary        = win->_pary;
	nwin->_parent      = win->_parent;

	nwin->_regtop      = win->_regtop;
	nwin->_regbottom   = win->_regbottom;

	linesize = (win->_maxx + 1) * sizeof(chtype);
	for (i = 0; i <= nwin->_maxy; i++) {
		memcpy(nwin->_line[i].text, win->_line[i].text, linesize);
		nwin->_line[i].firstchar  = win->_line[i].firstchar;
		nwin->_line[i].lastchar = win->_line[i].lastchar;
	}

	returnWin(nwin);
}
