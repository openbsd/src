
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
* Module menu_new                                                          *
* Creation and destruction of new menus                                    *
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  MENU *new_menu(ITEM **items)
|   
|   Description   :  Creates a new menu connected to the item pointer
|                    array items and returns a pointer to the new menu.
|                    The new menu is initialized with the values from the
|                    default menu.
|
|   Return Values :  NULL on error
+--------------------------------------------------------------------------*/
MENU *new_menu(ITEM ** items)
{
  MENU *menu = (MENU *)calloc(1,sizeof(MENU));
  
  if (menu)
    {
      *menu = _nc_Default_Menu;
      menu->rows = menu->frows;
      menu->cols = menu->fcols;
      if (items && *items)
	{
	  if (!_nc_Connect_Items(menu,items))
	    {
	      free(menu);
	      menu = (MENU *)0;
	    }
	}
    }

  if (!menu)
    SET_ERROR(E_SYSTEM_ERROR);

  return(menu);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int free_menu(MENU *menu)  
|   
|   Description   :  Disconnects menu from its associated item pointer 
|                    array and frees the storage allocated for the menu.
|
|   Return Values :  E_OK               - success
|                    E_BAD_ARGUMENT     - Invalid menu pointer passed
|                    E_POSTED           - Menu is already posted
+--------------------------------------------------------------------------*/
int free_menu(MENU * menu)
{
  if (!menu)
    RETURN(E_BAD_ARGUMENT);
  
  if ( menu->status & _POSTED )
    RETURN(E_POSTED);
  
  if (menu->items) 
    _nc_Disconnect_Items(menu);
  
  free(menu);
  RETURN(E_OK);
}

/* m_new.c ends here */
