/*	$OpenBSD: p_hide.c,v 1.1 1997/12/03 05:17:53 millert Exp $	*/

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

/* p_hide.c
 * Remove a panel from the stack
 */
#include "panel.priv.h"

MODULE_ID("Id: p_hide.c,v 1.1 1997/10/12 13:16:22 juergen Exp $")

/*+-------------------------------------------------------------------------
	__panel_unlink(pan) - unlink panel from stack
--------------------------------------------------------------------------*/
static void
__panel_unlink(PANEL *pan)
{
  PANEL *prev;
  PANEL *next;

#ifdef TRACE
  dStack("<u%d>",1,pan);
  if(!_nc_panel_is_linked(pan))
    return;
#endif

  _nc_override(pan,P_TOUCH);
  _nc_free_obscure(pan);

  prev = pan->below;
  next = pan->above;

  if(prev)
    { /* if non-zero, we will not update the list head */
      prev->above = next;
      if(next)
	next->below = prev;
    }
  else if(next)
    next->below = prev;
  if(pan == _nc_bottom_panel)
    _nc_bottom_panel = next;
  if(pan == _nc_top_panel)
    _nc_top_panel = prev;

  _nc_calculate_obscure();

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  dStack("<u%d>",9,pan);
}

int
hide_panel(register PANEL *pan)
{
  if(!pan)
    return(ERR);

  dBug(("--> hide_panel %s", USER_PTR(pan->user)));

  if(!_nc_panel_is_linked(pan))
    {
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      return(ERR);
    }

  __panel_unlink(pan);
  return(OK);
}
