
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

/***************************************************************************
* Module menu_items                                                        *
* Connect and disconnect items to and from menus                           *
***************************************************************************/

#include "menu.priv.h"

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
