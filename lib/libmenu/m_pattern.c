/*	$OpenBSD: m_pattern.c,v 1.1 1997/12/03 05:31:25 millert Exp $	*/

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
* Module m_pattern                                                         *
* Pattern matching handling                                                *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_pattern.c,v 1.1 1997/10/21 08:44:31 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *menu_pattern(const MENU *menu)
|   
|   Description   :  Return the value of the pattern buffer.
|
|   Return Values :  NULL          - if there is no pattern buffer allocated
|                    EmptyString   - if there is a pattern buffer but no
|                                    pattern is stored
|                    PatternString - as expected
+--------------------------------------------------------------------------*/
char *menu_pattern(const MENU * menu)
{
  return (menu ? (menu->pattern ? menu->pattern : "") : (char *)0);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_pattern(MENU *menu, const char *p)
|   
|   Description   :  Set the match pattern for a menu and position to the
|                    first item that matches.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid menu or pattern pointer
|                    E_NOT_CONNECTED   - no items connected to menu
|                    E_BAD_STATE       - menu in user hook routine
|                    E_NO_MATCH        - no item matches pattern
+--------------------------------------------------------------------------*/
int set_menu_pattern(MENU *menu, const char *p)
{
  ITEM *matchitem;
  int   matchpos;
  
  if (!menu || !p)	
    RETURN(E_BAD_ARGUMENT);
  
  if (!(menu->items))
    RETURN(E_NOT_CONNECTED);
  
  if ( menu->status & _IN_DRIVER )
    RETURN(E_BAD_STATE);
  
  Reset_Pattern(menu);
  
  if (!(*p))
    {
      pos_menu_cursor(menu);
      RETURN(E_OK);
    }
  
  if (menu->status & _LINK_NEEDED) 
    _nc_Link_Items(menu);
  
  matchpos  = menu->toprow;
  matchitem = menu->curitem;
  assert(matchitem);
  
  while(*p)
    {
      if ( !isprint(*p) || 
	  (_nc_Match_Next_Character_In_Item_Name(menu,*p,&matchitem) != E_OK) )
	{
	  Reset_Pattern(menu);
	  pos_menu_cursor(menu);
	  RETURN(E_NO_MATCH);
	}
      p++;
    }			
  
  /* This is reached if there was a match. So we position to the new item */
  Adjust_Current_Item(menu,matchpos,matchitem);
  RETURN(E_OK);
}

/* m_pattern.c ends here */
