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
#define _MARK_ALLOCATED (0x08)

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
  {wmove((menu)->win,(menu)->spc_rows*(item)->y,((menu)->itemlen+(menu)->spc_cols)*(item)->x);\
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
    if ( (item)->y >= (row + (menu)->arows) )\
      row = ( (item)->y < ((menu)->rows - row) ) ? \
            (item)->y : (menu)->rows - (menu)->arows;\
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
