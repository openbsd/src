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
* Module menu_item_new                                                     *
* Create and destroy menu items                                            *
* Set and get marker string for menu
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_item_new.c,v 1.5 1997/05/01 16:47:26 juergen Exp $")

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
ITEM *new_item(const char *name, const char *description)
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
	  
	  item->name.length	   = strlen(name);
	  item->name.str 	   = (char *)malloc(1 + item->name.length);
	  if (item->name.str)
	    {
	      strcpy(item->name.str, name);
	    }
	  else
	    {
	      free(item);
	      SET_ERROR( E_SYSTEM_ERROR );
	      return (ITEM *)0;
	    }
	  
	  if (description && (*description != '\0') && 
	      Is_Printable_String(description))
	    {
	      item->description.length = strlen(description);	      
	      item->description.str    = 
		(char *)malloc(1 + item->description.length);
	      if (item->description.str)
		{
		  strcpy(item->description.str, description);
		}
	      else
		{
		  free(item->name.str);
		  free(item);
		  SET_ERROR( E_SYSTEM_ERROR );
		  return (ITEM *)0;
		}
	    }
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
  
  if (item->name.str)
    free(item->name.str);
  if (item->description.str)
    free (item->description.str);
  free(item);

  RETURN( E_OK );
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_menu_mark( MENU *menu, const char *mark )
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
|                    E_SYSTEM_ERROR     - no memory to store mark
+--------------------------------------------------------------------------*/
int set_menu_mark(MENU * menu, const char * mark)
{
  int l;

  if ( mark && (*mark != '\0') && Is_Printable_String(mark) )
    l = strlen(mark);
  else
    l = 0;

  if ( menu )
    {
      char *old_mark = menu->mark;
      unsigned short old_status = menu->status;

      if (menu->status & _POSTED)
	{
	  /* If the menu is already posted, the geometry is fixed. Then
	     we can only accept a mark with exactly the same length */
	  if (menu->marklen != l) 
	    RETURN(E_BAD_ARGUMENT);
	}	
      menu->marklen = l;
      if (l)
	{
	  menu->mark = (char *)malloc(l+1);
	  if (menu->mark)
	    {
	      strcpy(menu->mark, mark);
	      menu->status |= _MARK_ALLOCATED;
	    }
	  else
	    {
	      menu->mark = old_mark;
	      RETURN(E_SYSTEM_ERROR);
	    }
	}
      else
	menu->mark = (char *)0;
      
      if ((old_status & _MARK_ALLOCATED) && old_mark)
	free(old_mark);

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
      return set_menu_mark(&_nc_Default_Menu, mark);
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
const char *menu_mark(const MENU * menu)
{
  return Normalize_Menu( menu )->mark;
}

/* m_item_new.c */
