
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
* Module menu_hook                                                         *
* Assign application specific routines for automatic invocation by menus   *
***************************************************************************/

#include "menu.priv.h"

/* "Template" macro to generate function to set application specific hook */
#define GEN_HOOK_SET_FUNCTION( typ, name ) \
int set_ ## typ ## _ ## name (MENU *menu, Menu_Hook func )\
{\
   (Normalize_Menu(menu) -> typ ## name = func );\
   RETURN(E_OK);\
}

/* "Template" macro to generate function to get application specific hook */
#define GEN_HOOK_GET_FUNCTION( typ, name ) \
Menu_Hook typ ## _ ## name ( const MENU *menu )\
{\
   return (Normalize_Menu(menu) -> typ ## name);\
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_init(MENU *menu, void (*f)(MENU *))
|   
|   Description   :  Set user-exit which is called when menu is posted
|                    or just after the top row changes.
|
|   Return Values :  E_OK               - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION( menu, init )		  

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void (*)(MENU *) menu_init(const MENU *menu)
|   
|   Description   :  Return address of user-exit function which is called
|                    when a menu is posted or just after the top row 
|                    changes.
|
|   Return Values :  Menu init function address or NULL
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION( menu, init )

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_term (MENU *menu, void (*f)(MENU *))
|   
|   Description   :  Set user-exit which is called when menu is unposted
|                    or just before the top row changes.
|
|   Return Values :  E_OK               - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION( menu, term )		  

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void (*)(MENU *) menu_term(const MENU *menu)
|   
|   Description   :  Return address of user-exit function which is called
|                    when a menu is unposted or just before the top row 
|                    changes.
|
|   Return Values :  Menu finalization function address or NULL
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION( menu, term )

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_item_init (MENU *menu, void (*f)(MENU *))
|   
|   Description   :  Set user-exit which is called when menu is posted
|                    or just after the current item changes.
|
|   Return Values :  E_OK               - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION( item, init )		  

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void (*)(MENU *) item_init (const MENU *menu)
|   
|   Description   :  Return address of user-exit function which is called
|                    when a menu is posted or just after the current item 
|                    changes.
|
|   Return Values :  Item init function address or NULL
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION( item, init )

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_item_term (MENU *menu, void (*f)(MENU *))
|   
|   Description   :  Set user-exit which is called when menu is unposted
|                    or just before the current item changes.
|
|   Return Values :  E_OK               - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION( item, term )		  

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void (*)(MENU *) item_init (const MENU *menu)
|   
|   Description   :  Return address of user-exit function which is called
|                    when a menu is unposted or just before the current item 
|                    changes.
|
|   Return Values :  Item finalization function address or NULL
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION( item, term )

/* m_hook.c ends here */
