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
* Module menu_item_val                                                     *
* Set and get menus item values                                            *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_val.c,v 1.4 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_item_value(ITEM *item, int value)
|   
|   Description   :  Programmatically set the items selection value. This is
|                    only allowed if the item is selectable at all and if
|                    it is not connected to a single-valued menu.
|                    If the item is connected to a posted menu, the menu
|                    will be redisplayed.  
|
|   Return Values :  E_OK              - success
|                    E_REQUEST_DENIED  - not selectable or single valued menu
+--------------------------------------------------------------------------*/
int set_item_value(ITEM *item, bool value)
{
  MENU *menu;
  
  if (item)
    {
      menu = item->imenu;
      
      if ((!(item->opt & O_SELECTABLE)) ||
	  (menu && (menu->opt & O_ONEVALUE))) 
	RETURN(E_REQUEST_DENIED);
      
      if (item->value ^ value)
	{
	  item->value = value ? TRUE : FALSE;
	  if (menu)
	    {
	      if (menu->status & _POSTED)
		{
		  Move_And_Post_Item(menu,item);
		  _nc_Show_Menu(menu);
		}
	    }
	}
    }
  else
    _nc_Default_Item.value = value;
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  bool item_value(const ITEM *item)
|   
|   Description   :  Return the selection value of the item
|
|   Return Values :  TRUE   - if item is selected
|                    FALSE  - if item is not selected
+--------------------------------------------------------------------------*/
bool item_value(const ITEM *item)
{
  return ((Normalize_Item(item)->value) ? TRUE : FALSE);
}

/* m_item_val.c ends here */
