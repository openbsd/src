/*	$OpenBSD: p_new.c,v 1.1 1997/12/03 05:17:54 millert Exp $	*/

/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                     panels is copyright (C) 1995                         *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*	      All praise to the original author, Warren Tucker.            *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute panels   *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of panels in any    *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        panels comes AS IS with no warranty, implied or expressed.        *
*                                                                          *
***************************************************************************/

/* p_new.c
 * Creation of a new panel 
 */
#include "panel.priv.h"

MODULE_ID("Id: p_new.c,v 1.1 1997/10/12 13:16:22 juergen Exp $")

/*+-------------------------------------------------------------------------
  Get root (i.e. stdscr's) panel.
  Establish the pseudo panel for stdscr if necessary.
--------------------------------------------------------------------------*/
static PANEL*
root_panel(void)
{
  if(_nc_stdscr_pseudo_panel == (PANEL*)0)
    {
      
      assert(stdscr && !_nc_bottom_panel && !_nc_top_panel);
      _nc_stdscr_pseudo_panel = (PANEL*)malloc(sizeof(PANEL));
      if (_nc_stdscr_pseudo_panel != 0) {
	PANEL* pan  = _nc_stdscr_pseudo_panel;
	WINDOW* win = stdscr;
	pan->win = win;
	getbegyx(win, pan->wstarty, pan->wstartx);
	pan->wendy   = pan->wstarty + getmaxy(win);
	pan->wendx   = pan->wstartx + getmaxx(win);
	pan->below   = (PANEL*)0;
	pan->above   = (PANEL*)0;
        pan->obscure = (PANELCONS*)0;
#ifdef TRACE
	pan->user = "stdscr";
#else
	pan->user = (void*)0;
#endif
	_nc_panel_link_bottom(pan);
      }
    }
  return _nc_stdscr_pseudo_panel;
}

PANEL *
new_panel(WINDOW *win)
{
  PANEL *pan = (PANEL*)0;

  (void)root_panel();
  assert(_nc_stdscr_pseudo_panel);

  if (!(win->_flags & _ISPAD) && (pan = (PANEL*)malloc(sizeof(PANEL))))
    {
      pan->win = win;
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      getbegyx(win, pan->wstarty, pan->wstartx);
      pan->wendy = pan->wstarty + getmaxy(win);
      pan->wendx = pan->wstartx + getmaxx(win);
#ifdef TRACE
      pan->user = "new";
#else
      pan->user = (char *)0;
#endif
      pan->obscure = (PANELCONS *)0;
      (void)show_panel(pan);
    }
  return(pan);
}
