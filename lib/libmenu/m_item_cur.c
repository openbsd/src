
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
* Module menu_item_cur                                                     *
* Set and get current menus item                                           *
***************************************************************************/

#include "menu.priv.h"

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
|   Return Values :  The index or -1 if this is an invalid item pointer
+--------------------------------------------------------------------------*/
int item_index(const ITEM *item)
{
  return (item && item->imenu) ? item->index : -1;
}

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
      
      if ((row<0) || (row>=(menu->rows - menu->height)))
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
|   Return Values :  The row number or -1 if there is no row
+--------------------------------------------------------------------------*/
int top_row(const MENU * menu)
{
  if (menu && menu->items && *(menu->items))
    {
      assert( (menu->toprow>=0) && (menu->toprow < menu->rows) );
      return menu->toprow;
    }
  else
    return(-1);
}

/* m_item_cur.c ends here */
