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
* Module menu_new                                                          *
* Creation and destruction of new menus                                    *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_new.c,v 1.5 1997/05/01 16:47:26 juergen Exp $")

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
  
  if ((menu->status & _MARK_ALLOCATED) && menu->mark)
    free(menu->mark);

  free(menu);
  RETURN(E_OK);
}

/* m_new.c ends here */
