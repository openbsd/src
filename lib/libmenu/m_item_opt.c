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
* Module menu_item_opt                                                    *
* Menus item option routines                                               *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_opt.c,v 1.4 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_item_opts(ITEM *item, Item_Options opts)  
|   
|   Description   :  Set the options of the item. If there are relevant
|                    changes, the item is connected and the menu is posted,
|                    the menu will be redisplayed.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid item options
+--------------------------------------------------------------------------*/
int set_item_opts(ITEM *item, Item_Options opts)
{ 
  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  
  if (item)
    {
      if (item->opt != opts)
	{		
	  MENU *menu = item->imenu;
	  
	  item->opt = opts;
	  
	  if ((!(opts & O_SELECTABLE)) && item->value)
	    item->value = FALSE;
	  
	  if (menu && (menu->status & _POSTED))
	    {
	      Move_And_Post_Item( menu, item );
	      _nc_Show_Menu(menu);
	    }
	}
    }
  else
    _nc_Default_Item.opt = opts;
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int item_opts_off(ITEM *item, Item_Options opts)   
|   
|   Description   :  Switch of the options for this item.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid options
+--------------------------------------------------------------------------*/
int item_opts_off(ITEM *item, Item_Options  opts)
{ 
  ITEM *citem = item; /* use a copy because set_item_opts must detect
                         NULL item itself to adjust its behaviour */

  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Item(citem);
      opts = citem->opt & ~opts;
      return set_item_opts( item, opts );
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int item_opts_on(ITEM *item, Item_Options opts)   
|   
|   Description   :  Switch on the options for this item.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid options
+--------------------------------------------------------------------------*/
int item_opts_on(ITEM *item, Item_Options opts)
{
  ITEM *citem = item; /* use a copy because set_item_opts must detect
                         NULL item itself to adjust its behaviour */
  
  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Item(citem);
      opts = citem->opt | opts;
      return set_item_opts( item, opts );
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  Item_Options item_opts(const ITEM *item)   
|   
|   Description   :  Switch of the options for this item.
|
|   Return Values :  Items options
+--------------------------------------------------------------------------*/
Item_Options item_opts(const ITEM * item)
{
  return (ALL_ITEM_OPTS & Normalize_Item(item)->opt);
}

/* m_item_opt.c ends here */
