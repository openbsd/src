
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
* Module menu_item_opt                                                    *
* Menus item option routines                                               *
***************************************************************************/

#include "menu.priv.h"

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
