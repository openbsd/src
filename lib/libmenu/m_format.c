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
* Module menu_format                                                       *
* Set and get maximum numbers of rows and columns in menus                 *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_format.c,v 1.5 1997/05/01 16:47:26 juergen Exp $")

#define minimum(a,b) ((a)<(b) ? (a): (b))

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_format(MENU *menu, int rows, int cols)
|   
|   Description   :  Sets the maximum number of rows and columns of items
|                    that may be displayed at one time on a menu. If the
|                    menu contains more items than can be displayed at
|                    once, the menu will be scrollable.
|
|   Return Values :  E_OK                   - success
|                    E_BAD_ARGUMENT         - invalid values passed
|                    E_NOT_CONNECTED        - there are no items connected
|                    E_POSTED               - the menu is already posted
+--------------------------------------------------------------------------*/
int set_menu_format(MENU *menu, int rows, int cols)
{
  int total_rows, total_cols;
  
  if (rows<0 || cols<0) 
    RETURN(E_BAD_ARGUMENT);
  
  if (menu)
    {
      if ( menu->status & _POSTED )
	RETURN(E_POSTED);
      
      if (!(menu->items))
	RETURN(E_NOT_CONNECTED);
      
      if (rows==0) 
	rows = menu->frows;
      if (cols==0) 
	cols = menu->fcols;
      
      if (menu->pattern)
	Reset_Pattern(menu);
      
      menu->frows = rows;
      menu->fcols = cols;
      
      assert(rows>0 && cols>0);
      total_rows = (menu->nitems - 1)/cols + 1;
      total_cols = (menu->status & O_ROWMAJOR) ? 
	minimum(menu->nitems,cols) :
	  (menu->nitems-1)/total_rows + 1;
      
      menu->rows    = total_rows;
      menu->cols    = total_cols;
      menu->arows   = minimum(total_rows,rows); 
      menu->toprow  = 0;	
      menu->curitem = *(menu->items);
      assert(menu->curitem);
      menu->status |= _LINK_NEEDED;
      _nc_Calculate_Item_Length_and_Width(menu);
    }
  else
    {
      if (rows>0) _nc_Default_Menu.frows = rows;
      if (cols>0) _nc_Default_Menu.fcols = cols;
    }
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void menu_format(const MENU *menu, int *rows, int *cols)
|   
|   Description   :  Returns the maximum number of rows and columns that may
|                    be displayed at one time on menu.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
void menu_format(const MENU *menu, int *rows, int *cols)
{
  if (rows)
    *rows = Normalize_Menu(menu)->frows;
  if (cols)
    *cols = Normalize_Menu(menu)->fcols;
}

/* m_format.c ends here */
