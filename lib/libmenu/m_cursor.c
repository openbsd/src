/*	$OpenBSD: m_cursor.c,v 1.4 1998/07/24 16:38:53 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <Juergen.Pfeifer@T-Online.de> 1995,1997        *
 ****************************************************************************/

/***************************************************************************
* Module m_cursor                                                          *
* Correctly position a menus cursor                                        *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("$From: m_cursor.c,v 1.9 1998/02/11 12:13:50 tom Exp $")

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
