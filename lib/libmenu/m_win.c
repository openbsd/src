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
* Module menu_win                                                          *
* Menus window and subwindow association routines                          *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_win.c,v 1.5 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_win(MENU *menu, WINDOW *win)
|   
|   Description   :  Sets the window of the menu.
|
|   Return Values :  E_OK               - success
|                    E_POSTED           - menu is already posted
+--------------------------------------------------------------------------*/
int set_menu_win(MENU *menu, WINDOW *win)
{
  if (menu)
    {
      if ( menu->status & _POSTED )
	RETURN(E_POSTED);
      menu->userwin = win;
      _nc_Calculate_Item_Length_and_Width(menu);
    }
  else
    _nc_Default_Menu.userwin = win;
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  WINDOW *menu_win(const MENU *)
|   
|   Description   :  Returns pointer to the window of the menu
|
|   Return Values :  NULL on error, otherwise pointer to window
+--------------------------------------------------------------------------*/
WINDOW *menu_win(const MENU *menu)
{
  const MENU* m = Normalize_Menu(menu);
  return (m->userwin ? m->userwin : stdscr);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_sub(MENU *menu, WINDOW *win)
|   
|   Description   :  Sets the subwindow of the menu.
|
|   Return Values :  E_OK           - success
|                    E_POSTED       - menu is already posted
+--------------------------------------------------------------------------*/
int set_menu_sub(MENU *menu, WINDOW *win)
{
  if (menu)
    {
      if ( menu->status & _POSTED )
	RETURN(E_POSTED);
      menu->usersub = win;
      _nc_Calculate_Item_Length_and_Width(menu);
    }
  else
    _nc_Default_Menu.usersub = win;
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  WINDOW *menu_sub(const MENU *menu)
|   
|   Description   :  Returns a pointer to the subwindow of the menu
|
|   Return Values :  NULL on error, otherwise a pointer to the window
+--------------------------------------------------------------------------*/
WINDOW *menu_sub(const MENU * menu)
{
  const MENU* m = Normalize_Menu(menu);
  return Get_Menu_Window(m);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int scale_menu(const MENU *menu)
|   
|   Description   :  Returns the minimum window size necessary for the
|                    subwindow of menu.  
|
|   Return Values :  E_OK                  - success
|                    E_BAD_ARGUMENT        - invalid menu pointer
|                    E_NOT_CONNECTED       - no items are connected to menu
+--------------------------------------------------------------------------*/
int scale_menu(const MENU *menu, int *rows, int *cols)
{
  if (!menu) 
    RETURN( E_BAD_ARGUMENT );
  
  if (menu->items && *(menu->items))
    {
      if (rows)
	*rows = menu->height;
      if (cols)
	*cols = menu->width;
      RETURN(E_OK);
    }
  else
    RETURN( E_NOT_CONNECTED );
}

/* m_win.c ends here */
