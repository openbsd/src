
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
* Module menu_item_use                                                     *
* Associate application data with menu items                               *
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_item_userptr(ITEM *item, char *userptr)
|   
|   Description   :  Set the pointer that is reserved in any item to store
|                    application relevant informations.  
|
|   Return Values :  E_OK               - success
+--------------------------------------------------------------------------*/
int set_item_userptr(ITEM * item, char * userptr)
{
  Normalize_Item(item)->userptr = userptr;
  RETURN( E_OK );
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *item_userptr(const ITEM *item)
|   
|   Description   :  Return the pointer that is reserved in any item to store
|                    application relevant informations.
|
|   Return Values :  Value of the pointer. If no such pointer has been set,
|                    NULL is returned.
+--------------------------------------------------------------------------*/
char *item_userptr(const ITEM * item)
{
  return Normalize_Item(item)->userptr;
}

/* m_item_use.c */
