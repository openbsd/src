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
* Module menu_opts                                                         *
* Menus option routines                                                    *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_opts.c,v 1.6 1997/05/01 16:47:26 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  int set_menu_opts(MENU *menu, Menu_Options opts)
|
|   Description   :  Set the options for this menu. If the new settings
|                    end up in a change of the geometry of the menu, it
|                    will be recalculated. This operation is forbidden if
|                    the menu is already posted.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid menu options
|                    E_POSTED       - menu is already posted
+--------------------------------------------------------------------------*/
int set_menu_opts(MENU * menu, Menu_Options opts)
{
  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);

  if (menu)
    {
      if ( menu->status & _POSTED )
	RETURN(E_POSTED);

      if ( (opts&O_ROWMAJOR) != (menu->opt&O_ROWMAJOR))
	{
	  /* we need this only if the layout really changed ... */
	  if (menu->items && menu->items[0])
	    {
	      menu->toprow  = 0;
	      menu->curitem = menu->items[0];
	      assert(menu->curitem);
	      set_menu_format( menu, menu->frows, menu->fcols );
	    }
	}

      menu->opt = opts;

      if (opts & O_ONEVALUE)
	{
	  ITEM **item;

	  if ( ((item=menu->items) != (ITEM**)0) )
	    for(;*item;item++)
	      (*item)->value = FALSE;
	}

      if (opts & O_SHOWDESC)	/* this also changes the geometry */
	_nc_Calculate_Item_Length_and_Width( menu );
    }
  else
    _nc_Default_Menu.opt = opts;

  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  int menu_opts_off(MENU *menu, Menu_Options opts)
|
|   Description   :  Switch off the options for this menu. If the new settings
|                    end up in a change of the geometry of the menu, it
|                    will be recalculated. This operation is forbidden if
|                    the menu is already posted.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid options
|                    E_POSTED       - menu is already posted
+--------------------------------------------------------------------------*/
int menu_opts_off(MENU *menu, Menu_Options  opts)
{
  MENU *cmenu = menu; /* use a copy because set_menu_opts must detect
                         NULL menu itself to adjust its behaviour */

  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Menu(cmenu);
      opts = cmenu->opt & ~opts;
      return set_menu_opts( menu, opts );
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  int menu_opts_on(MENU *menu, Menu_Options opts)
|
|   Description   :  Switch on the options for this menu. If the new settings
|                    end up in a change of the geometry of the menu, it
|                    will be recalculated. This operation is forbidden if
|                    the menu is already posted.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid menu options
|                    E_POSTED       - menu is already posted
+--------------------------------------------------------------------------*/
int menu_opts_on(MENU * menu, Menu_Options opts)
{
  MENU *cmenu = menu; /* use a copy because set_menu_opts must detect
                         NULL menu itself to adjust its behaviour */

  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Menu(cmenu);
      opts = cmenu->opt | opts;
      return set_menu_opts(menu, opts);
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  Menu_Options menu_opts(const MENU *menu)
|
|   Description   :  Return the options for this menu.
|
|   Return Values :  Menu options
+--------------------------------------------------------------------------*/
Menu_Options menu_opts(const MENU *menu)
{
  return (ALL_MENU_OPTS & Normalize_Menu( menu )->opt);
}

/* m_opts.c ends here */
