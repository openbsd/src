/*	$OpenBSD: m_item_cur.c,v 1.3 1997/12/03 05:31:19 millert Exp $	*/

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
* Module m_item_cur                                                        *
* Set and get current menus item                                           *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_cur.c,v 1.8 1997/10/21 08:44:31 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_current_item(MENU *menu, const ITEM *item)
|   
|   Description   :  Make the item the current item
|
|   Return Values :  E_OK                - success
+--------------------------------------------------------------------------*/
int set_current_item(MENU * menu, ITEM * item)
{
  if (menu && item && (item->imenu==menu))
    {
      if ( menu->status & _IN_DRIVER )
	RETURN(E_BAD_STATE);
      
      assert( menu->curitem );
      if (item != menu->curitem)
	{
	  if (menu->status & _LINK_NEEDED)
	    {
	      /*
	       * Items are available, but they are not linked together.
	       * So we have to link here.
	       */
	      _nc_Link_Items(menu);
	    }
	  assert(menu->pattern);
	  Reset_Pattern(menu);
	  /* adjust the window to make item visible and update the menu */
	  Adjust_Current_Item(menu,menu->toprow,item);
	}
    }
  else
    RETURN(E_BAD_ARGUMENT);
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  ITEM *current_item(const MENU *menu)
|   
|   Description   :  Return the menus current item
|
|   Return Values :  Item pointer or NULL if failure
+--------------------------------------------------------------------------*/
ITEM *current_item(const MENU * menu) 
{
  return (menu && menu->items) ? menu->curitem : (ITEM *)0;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int item_index(const ITEM *)
|   
|   Description   :  Return the logical index of this item.
|
|   Return Values :  The index or ERR if this is an invalid item pointer
+--------------------------------------------------------------------------*/
int item_index(const ITEM *item)
{
  return (item && item->imenu) ? item->index : ERR;
}

/* m_item_cur.c ends here */
