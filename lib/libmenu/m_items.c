/*	$OpenBSD: m_items.c,v 1.3 1997/12/03 05:31:23 millert Exp $	*/

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
* Module m_items                                                           *
* Connect and disconnect items to and from menus                           *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_items.c,v 1.5 1997/10/21 08:44:31 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_items(MENU *menu, ITEM **items)
|   
|   Description   :  Sets the item pointer array connected to menu.
|
|   Return Values :  E_OK           - success
|                    E_POSTED       - menu is already posted
|                    E_CONNECTED    - one or more items are already connected
|                                     to another menu.
|                    E_BAD_ARGUMENT - An incorrect menu or item array was
|                                     passed to the function
+--------------------------------------------------------------------------*/
int set_menu_items(MENU * menu, ITEM ** items)
{
  if (!menu || (items && !(*items)))
    RETURN(E_BAD_ARGUMENT);
  
  if ( menu->status & _POSTED )
    RETURN(E_POSTED);
  
  if (menu->items)
    _nc_Disconnect_Items(menu);
  
  if (items)
    {
      if(!_nc_Connect_Items( menu, items )) 
	RETURN(E_CONNECTED);
    }
  
  menu->items = items;
  RETURN(E_OK);
}		

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  ITEM **menu_items(const MENU *menu)
|   
|   Description   :  Returns a pointer to the item pointer arry of the menu
|
|   Return Values :  NULL on error
+--------------------------------------------------------------------------*/
ITEM **menu_items(const MENU *menu)
{
  return(menu ? menu->items : (ITEM **)0);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int item_count(const MENU *menu)
|   
|   Description   :  Get the number of items connected to the menu. If the
|                    menu pointer is NULL we return -1.         
|
|   Return Values :  Number of items or -1 to indicate error.
+--------------------------------------------------------------------------*/
int item_count(const MENU *menu)
{
  return(menu ? menu->nitems : -1);
}

/* m_items.c ends here */
