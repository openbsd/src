/*	$OpenBSD: p_hidden.c,v 1.1 1997/12/03 05:17:52 millert Exp $	*/

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

/* p_hidden.c
 * Test whether or not panel is hidden
 */
#include "panel.priv.h"

MODULE_ID("Id: p_hidden.c,v 1.1 1997/10/12 13:16:22 juergen Exp $")

int
panel_hidden(const PANEL *pan)
{
  if(!pan)
    return(ERR);
  return(_nc_panel_is_linked(pan) ? TRUE : FALSE);
} 
