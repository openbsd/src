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
* Module menu_item_nam                                                     *
* Get menus item name and description                                      *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_nam.c,v 1.5 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *item_name(const ITEM *item)
|   
|   Description   :  Return name of menu item
|
|   Return Values :  See above; returns NULL if item is invalid
+--------------------------------------------------------------------------*/
const char *item_name(const ITEM * item) 
{
  return ((item) ? item->name.str : (char *)0);
}
		
/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *item_description(const ITEM *item)
|   
|   Description   :  Returns description of item
|
|   Return Values :  See above; Returns NULL if item is invalid
+--------------------------------------------------------------------------*/
const char *item_description(const ITEM * item)
{
  return ((item) ? item->description.str : (char *)0);
}

/* m_item_nam.c ends here */
