
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
* Module menu_format                                                       *
* Set and get maximum numbers of rows and columns in menus                 *
***************************************************************************/

#include "menu.priv.h"

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
      menu->height  = minimum(total_rows,rows); 
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
