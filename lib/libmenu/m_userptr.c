
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
* Module menu_userptr                                                      *
* Associate application data with menus                                    *
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_userptr(MENU *menu, char *userptr)
|   
|   Description   :  Set the pointer that is reserved in any menu to store
|                    application relevant informations.
|
|   Return Values :  E_OK         - success
+--------------------------------------------------------------------------*/
int set_menu_userptr(MENU * menu, char * userptr)
{
  Normalize_Menu(menu)->userptr = userptr;
  RETURN( E_OK );
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *menu_userptr(const MENU *menu)
|   
|   Description   :  Return the pointer that is reserved in any menu to
|                    store application relevant informations.
|
|   Return Values :  Value of the pointer. If no such pointer has been set,
|                    NULL is returned
+--------------------------------------------------------------------------*/
char *menu_userptr(const MENU * menu)
{
  return( Normalize_Menu(menu)->userptr);
}

/* m_userptr.c ends here */
