/*	$OpenBSD: m_item_top.c,v 1.1 1997/12/03 05:31:21 millert Exp $	*/

/*-----------------------------------------------------------------------------+
|           The ncurses menu library is  Copyright (C) 1995-1997               |
|             by Juergen Pfeifer <Juergen.Pfeifer@T-Online.de>                 |
|                          All Rights Reserved.                                |
|                                                                              |
| Permission to use, copy, modify, and distribute this software and its        |
| documentation for any purpose and without fee is hereby granted, provided    |
| that the above copyright notice appear in all copies and that both that      |
| copyright notice and this permission notice appear in supporting             |
| documentation, and that the name of the above listed copyright holder(s) not |
| be used in advertising or publicity pertaining to distribution of the        |
| software without specific, written prior permission.                         | 
|                                                                              |
| THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO  |
| THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-  |
| NESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR   |
| ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RE- |
| SULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, |
| NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH    |
| THE USE OR PERFORMANCE OF THIS SOFTWARE.                                     |
+-----------------------------------------------------------------------------*/

/***************************************************************************
* Module m_item_top                                                        *
* Set and get top menus item                                               *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_top.c,v 1.1 1997/10/21 08:44:31 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_top_row(MENU *menu, int row)
|   
|   Description   :  Makes the speified row the top row in the menu
|
|   Return Values :  E_OK             - success
|                    E_BAD_ARGUMENT   - not a menu pointer or invalid row
|                    E_NOT_CONNECTED  - there are no items for the menu
+--------------------------------------------------------------------------*/
int set_top_row(MENU * menu, int row)
{
  ITEM *item;
  
  if (menu)
    {
      if ( menu->status & _IN_DRIVER )
	RETURN(E_BAD_STATE);
      if (menu->items == (ITEM **)0)
	RETURN(E_NOT_CONNECTED);
      
      if ((row<0) || (row > (menu->rows - menu->arows)))
	RETURN(E_BAD_ARGUMENT);
    }
  else
    RETURN(E_BAD_ARGUMENT);
  
  if (row != menu->toprow)
    {
      if (menu->status & _LINK_NEEDED) 
	_nc_Link_Items(menu);
      
      item = menu->items[ (menu->opt&O_ROWMAJOR) ? (row*menu->cols) : row ];
      assert(menu->pattern);
      Reset_Pattern(menu);
      _nc_New_TopRow_and_CurrentItem(menu, row, item);
    }
  
    RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int top_row(const MENU *)
|   
|   Description   :  Return the top row of the menu
|
|   Return Values :  The row number or ERR if there is no row
+--------------------------------------------------------------------------*/
int top_row(const MENU * menu)
{
  if (menu && menu->items && *(menu->items))
    {
      assert( (menu->toprow>=0) && (menu->toprow < menu->rows) );
      return menu->toprow;
    }
  else
    return(ERR);
}

/* m_item_top.c ends here */
