
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

#include "curses.priv.h"
#include <string.h>

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

   if (orig)
   {
      if (win->_parx==x && win->_pary==y)
	return OK;
      if (x<0 || y<0)
	return ERR;
      if ( (x+getmaxx(win) > getmaxx(orig)) ||
           (y+getmaxy(win) > getmaxy(orig)) )
        return ERR;
   }
   else
      return ERR;
   wsyncup(win);
   win->_parx = x;
   win->_pary = y;
   for(i=0;i<getmaxy(win);i++)
     win->_line[i].text = &(orig->_line[y++].text[x]);
   return OK;	
}

int syncok(WINDOW *win, bool bf)
/* enable/disable automatic wsyncup() on each change to window */
{
	if (win) {
		win->_sync = bf;
		return(OK);
	} else
		return(ERR);
}

void wsyncup(WINDOW *win)
/* mark changed every cell in win's ancestors that is changed in win */
{
  WINDOW	*wp;

  if (win->_parent)
    for (wp = win; wp->_parent; wp = wp->_parent)
    {
      int i;
      WINDOW *pp = wp->_parent;

      for (i = 0; i <= wp->_maxy; i++)
      {
	if (pp->_line[wp->_pary + i].firstchar >= 0
		&& pp->_line[wp->_pary + i].firstchar < wp->_line[i].firstchar)
	  wp->_line[i].firstchar = pp->_line[wp->_pary + i].firstchar;
	if (pp->_line[wp->_pary + i].lastchar > wp->_line[i].lastchar)
	  wp->_line[i].lastchar = pp->_line[wp->_pary + i].lastchar;
      }
    }
}

void wsyncdown(WINDOW *win)
/* mark changed every cell in win that is changed in any of its ancestors */ 
{
  WINDOW *wp;

  if (win->_parent)
    for (wp = win; wp->_parent; wp = wp->_parent)
    {
      int i;
      WINDOW *pp = wp->_parent;

      for (i = 0; i <= wp->_maxy; i++)
      {
	if (wp->_line[i].firstchar >= 0
		&& wp->_line[i].firstchar < pp->_line[wp->_pary + i].firstchar)
	  pp->_line[wp->_pary + i].firstchar = wp->_line[i].firstchar;
	if (wp->_line[i].lastchar > pp->_line[wp->_pary + i].lastchar)
	  pp->_line[wp->_pary + i].lastchar = wp->_line[i].lastchar;
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

	T(("dupwin(%p) called", win));

	if ((nwin = newwin(win->_maxy + 1, win->_maxx + 1, win->_begy, win->_begx)) == NULL)
		return NULL;

	nwin->_curx	   = win->_curx;
	nwin->_cury	   = win->_cury;
	nwin->_maxy	   = win->_maxy;
	nwin->_maxx	   = win->_maxx;       
	nwin->_begy	   = win->_begy;
	nwin->_begx	   = win->_begx;

	nwin->_flags	   = win->_flags;
	nwin->_attrs	   = win->_attrs;
	nwin->_bkgd	   = win->_bkgd; 

	nwin->_clear	   = win->_clear; 
	nwin->_scroll	   = win->_scroll;
	nwin->_leaveok	   = win->_leaveok;
	nwin->_use_keypad  = win->_use_keypad;
	nwin->_delay   	   = win->_delay;
	nwin->_immed	   = win->_immed;
	nwin->_sync	   = win->_sync;
	nwin->_parx	   = win->_parx;
	nwin->_pary	   = win->_pary;
	nwin->_parent	   = win->_parent; 

	nwin->_regtop	   = win->_regtop;
	nwin->_regbottom   = win->_regbottom;

	linesize = (win->_maxx + 1) * sizeof(chtype);
	for (i = 0; i <= nwin->_maxy; i++) {
		memcpy(nwin->_line[i].text, win->_line[i].text, linesize);
		nwin->_line[i].firstchar  = win->_line[i].firstchar;
		nwin->_line[i].lastchar = win->_line[i].lastchar;
	}

	return nwin;
}

