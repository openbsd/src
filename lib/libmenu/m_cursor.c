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
* Module menu_cursor                                                       *
* Correctly position a menus cursor                                        *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_cursor.c,v 1.7 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  pos_menu_cursor  
|   
|   Description   :  Position logical cursor to current item in menu
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid menu
|                    E_NOT_POSTED    - Menu is not posted
+--------------------------------------------------------------------------*/
int pos_menu_cursor(const MENU * menu)
{
  if (!menu)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      ITEM *item;
      int x, y;
      WINDOW *win, *sub;
      
      if ( !( menu->status & _POSTED ) )
	RETURN(E_NOT_POSTED);
      
      item = menu->curitem;
      assert(item);
      
      x = item->x * (menu->spc_cols + menu->itemlen);
      y = (item->y - menu->toprow) * menu->spc_rows;
      win = menu->userwin ? menu->userwin : stdscr;
      sub = menu->usersub ? menu->usersub : win;
      assert(win && sub);
      
      if ((menu->opt & O_SHOWMATCH) && (menu->pindex > 0))
	x += ( menu->pindex + menu->marklen - 1);
      
      wmove(sub,y,x);
      
      if ( win != sub )
	{
	  wcursyncup(sub);
	  wsyncup(sub);
	  untouchwin(sub);
	} 
    }
  RETURN(E_OK);
}

/* m_cursor.c ends here */
