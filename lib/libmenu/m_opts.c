
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
* Module menu_opts                                                         *
* Menus option routines                                                    *
***************************************************************************/

#include "menu.priv.h"

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
  
	  if ( (item=menu->items) )
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
