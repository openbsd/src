
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
* Module menu.priv.h                                                       *
* Top level private header file for all libnmenu modules                   *
***************************************************************************/

#include "mf_common.h"
#include "menu.h"

/* Backspace code */
#define BS (8)

extern ITEM _nc_Default_Item;
extern MENU _nc_Default_Menu;

/* Normalize item to default if none was given */
#define Normalize_Item( item ) ((item)=(item)?(item):&_nc_Default_Item)

/* Normalize menu to default if none was given */
#define Normalize_Menu( menu ) ((menu)=(menu)?(menu):&_nc_Default_Menu)

/* Normalize menu window */
#define Get_Menu_Window(  menu ) \
   ( (menu)->usersub  ? (menu)->usersub  : (\
     (menu)->userwin  ? (menu)->userwin  : stdscr ))

/* menu specific status flags */
#define _LINK_NEEDED    (0x04)

#define ALL_MENU_OPTS (                 \
		       O_ONEVALUE     | \
		       O_SHOWDESC     | \
		       O_ROWMAJOR     | \
		       O_IGNORECASE   | \
		       O_SHOWMATCH    | \
		       O_NONCYCLIC    )

#define ALL_ITEM_OPTS (O_SELECTABLE)

/* Move to the window position of an item and draw it */
#define Move_And_Post_Item(menu,item) \
  {wmove((menu)->win,(item)->y,((menu)->itemlen+1)*(item)->x);\
   _nc_Post_Item((menu),(item));}

#define Move_To_Current_Item(menu,item) \
  if ( (item) != (menu)->curitem)\
    {\
      Move_And_Post_Item(menu,item);\
      Move_And_Post_Item(menu,(menu)->curitem);\
    }

/* This macro ensures, that the item becomes visible, if possible with the
   specified row as the top row of the window. If this is not possible,
   the top row will be adjusted and the value is stored in the row argument. 
*/
#define Adjust_Current_Item(menu,row,item) \
  { if ((item)->y < row) \
      row = (item)->y;\
    if ( (item)->y >= (row + (menu)->height) )\
      row = ( (item)->y < ((menu)->rows - row) ) ? \
            (item)->y : (menu)->rows - (menu)->height;\
    _nc_New_TopRow_and_CurrentItem(menu,row,item); }

/* Reset the match pattern buffer */
#define Reset_Pattern(menu) \
  { (menu)->pindex = 0; \
    (menu)->pattern[0] = '\0'; }

/* Internal functions. */						
extern void _nc_Draw_Menu(const MENU *);
extern void _nc_Show_Menu(const MENU *);
extern void _nc_Calculate_Item_Length_and_Width(MENU *);
extern void _nc_Post_Item(const MENU *, const ITEM *);
extern bool _nc_Connect_Items(MENU *, ITEM **);
extern void _nc_Disconnect_Items(MENU *);
extern void _nc_New_TopRow_and_CurrentItem(MENU *,int, ITEM *);
extern void _nc_Link_Items(MENU *);
