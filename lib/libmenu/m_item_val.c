
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
* Module menu_item_val                                                     *
* Set and get menus item values                                            *
***************************************************************************/

#include "menu.priv.h"

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
