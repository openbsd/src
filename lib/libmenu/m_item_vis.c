
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
* Module menu_item_vis                                                     *
* Tell if menu item is visible                                             *
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  bool item_visible(const ITEM *item)
|   
|   Description   :  A item is visible if it currently appears in the
|                    subwindow of a posted menu.
|
|   Return Values :  TRUE  if visible
|                    FALSE if invisible
+--------------------------------------------------------------------------*/
bool item_visible(const ITEM * item)
{
  MENU *menu;
  
  if ( item                                               && 
      (menu=item->imenu)                                  && 
      (menu->status & _POSTED)                            &&
      ( (menu->toprow + menu->height - 1) >= (item->y) )  &&
      ( item->y >= menu->toprow) )
    return TRUE;
  else
    return FALSE;
}

/* m_item_vis.c ends here */
