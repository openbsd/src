
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
* Module menu_item_new                                                     *
* Create and destroy menu items                                            *
* Set and get marker string for menu
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  bool Is_Printable_String(const char *s)
|   
|   Description   :  Checks whether or not the string contains only printable
|                    characters.
|
|   Return Values :  TRUE     - if string is printable
|                    FALSE    - if string contains non-printable characters
+--------------------------------------------------------------------------*/
static bool Is_Printable_String(const char *s)
{
  assert(s);
  while(*s)
    {
      if (!isprint((unsigned char)*s))
	return FALSE;
      s++;
    }
  return TRUE;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  ITEM *new_item(char *name, char *description)
|   
|   Description   :  Create a new item with name and description. Return
|                    a pointer to this new item.
|                    N.B.: an item must(!) have a name.
|
|   Return Values :  The item pointer or NULL if creation failed.
+--------------------------------------------------------------------------*/
ITEM *new_item(char *name, char *description)
{
  ITEM *item;
  
  if ( !name || (*name == '\0') || !Is_Printable_String(name) )
    {
      item = (ITEM *)0;
      SET_ERROR( E_BAD_ARGUMENT );
    }
  else
    {
      item = (ITEM *)calloc(1,sizeof(ITEM));
      if (item)
	{
	  *item  = _nc_Default_Item; /* hope we have struct assignment */
	  
	  item->name.str 	   = name;
	  item->name.length	   = strlen(name);
	  
	  item->description.str    = description;
	  if (description && Is_Printable_String(description))
	    item->description.length = strlen(description);
	  else
	    {
	      item->description.length = 0;
	      item->description.str    = (char *)0;
	    }
	}
      else
	SET_ERROR( E_SYSTEM_ERROR );
    }  
  return(item);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int free_item(ITEM *item)
|   
|   Description   :  Free the allocated storage for this item. 
|                    N.B.: a connected item can't be freed.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid value has been passed
|                    E_CONNECTED       - item is still connected to a menu    
+--------------------------------------------------------------------------*/
int free_item(ITEM * item)
{
  if (!item)
    RETURN( E_BAD_ARGUMENT );

  if (item->imenu)
    RETURN( E_CONNECTED );
  
  free(item);
  RETURN( E_OK );
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_mark( MENU *menu, char *mark )
|   
|   Description   :  Set the mark string used to indicate the current
|                    item (single-valued menu) or the selected items
|                    (multi-valued menu).
|                    The mark argument may be NULL, in which case no 
|                    marker is used.
|                    This might be a little bit tricky, because this may 
|                    affect the geometry of the menu, which we don't allow 
|                    if it is already posted.
|
|   Return Values :  E_OK               - success
|                    E_BAD_ARGUMENT     - an invalid value has been passed
+--------------------------------------------------------------------------*/
int set_menu_mark(MENU * menu, char * mark)
{
  int l;
  
  if ( mark && *mark && Is_Printable_String(mark) )
    l = strlen(mark);
  else
    l = 0;
  
  if ( menu )
    {
      if (menu->status & _POSTED)
	{
	  /* If the menu is already posted, the geometry is fixed. Then
	     we can only accept a mark with exactly the same length */
	  if (menu->marklen != l) 
	    RETURN(E_BAD_ARGUMENT);
	}	
      menu->mark    = l ? mark : (char *)0;
      menu->marklen = l;
      
      if (menu->status & _POSTED)
	{
	  _nc_Draw_Menu( menu );
	  _nc_Show_Menu( menu );
	}
      else
	{
	  /* Recalculate the geometry */
	  _nc_Calculate_Item_Length_and_Width( menu );			
	}
    }
  else
    {
      _nc_Default_Menu.mark    = l ? mark : (char *)0;
      _nc_Default_Menu.marklen = l;
    }
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  char *menu_mark(const MENU *menu)
|   
|   Description   :  Return a pointer to the marker string
|
|   Return Values :  The marker string pointer or NULL if no marker defined
+--------------------------------------------------------------------------*/
char *menu_mark(const MENU * menu)
{
  return Normalize_Menu( menu )->mark;
}

/* m_item_new.c */
