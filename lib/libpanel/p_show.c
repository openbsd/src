/*	$OpenBSD: p_show.c,v 1.1 1997/12/03 05:17:54 millert Exp $	*/

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

/* p_show.c
 * Place a panel on top of the stack; may already be in the stack 
 */
#include "panel.priv.h"

MODULE_ID("Id: p_show.c,v 1.1 1997/10/12 13:16:22 juergen Exp $")

static void
panel_link_top(PANEL *pan)
{
#ifdef TRACE
  dStack("<lt%d>",1,pan);
  if(_nc_panel_is_linked(pan))
    return;
#endif

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  if(_nc_top_panel)
    {
      _nc_top_panel->above = pan;
      pan->below = _nc_top_panel;
    }
  _nc_top_panel = pan;
  if(!_nc_bottom_panel)
    _nc_bottom_panel = pan;
  _nc_calculate_obscure();
  dStack("<lt%d>",9,pan);
}

int
show_panel(PANEL *pan)
{
  if(!pan)
    return(ERR);
  if(pan == _nc_top_panel)
    return(OK);
  dBug(("--> show_panel %s", USER_PTR(pan->user)));
  if(_nc_panel_is_linked(pan))
    (void)hide_panel(pan);
  panel_link_top(pan);
  return(OK);
}
