
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
* Module menu_driver and menu_pattern                                      *
* Central dispatching routine and pattern matching handling                *
***************************************************************************/

#include "menu.priv.h"

/* Macros */

/* Remove the last character from the match pattern buffer */
#define Remove_Character_From_Pattern(menu) \
  (menu)->pattern[--((menu)->pindex)] = '\0'

/* Add a new character to the match pattern buffer */
#define Add_Character_To_Pattern(menu,ch) \
  { (menu)->pattern[((menu)->pindex)++] = (ch);\
    (menu)->pattern[(menu)->pindex] = '\0'; }

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  static bool Is_Sub_String( 
|                           bool IgnoreCaseFlag,
|                           const char *part,
|                           const char *string)
|   
|   Description   :  Checks whether or not part is a substring of string.
|
|   Return Values :  TRUE   - if it is a substring
|                    FALSE  - if it is not a substring
+--------------------------------------------------------------------------*/
static bool Is_Sub_String(
			  bool  IgnoreCaseFlag,
			  const char *part,
			  const char *string
			 )
{
  assert( part && string );
  if ( IgnoreCaseFlag )
    {
      while(*string && *part)
	{
	  if (toupper(*string++)!=toupper(*part)) break;
	  part++;
	}
    }
  else
    {
      while( *string && *part )
	if (*part != *string++) break;
      part++;
    }
  return ( (*part) ? FALSE : TRUE );
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  static int Match_Next_Character_In_Item_Name(
|                           MENU *menu,
|                           int  ch,
|                           ITEM **item)
|   
|   Description   :  This internal routine is called for a menu positioned
|                    at an item with three different classes of characters:
|                       - a printable character; the character is added to
|                         the current pattern and the next item matching
|                         this pattern is searched.
|                       - NUL; the pattern stays as it is and the next item
|                         matching the pattern is searched
|                       - BS; the pattern stays as it is and the previous
|                         item matching the pattern is searched
|
|                       The item parameter contains on call a pointer to
|                       the item where the search starts. On return - if
|                       a match was found - it contains a pointer to the
|                       matching item.
|  
|   Return Values :  E_OK        - an item matching the pattern was found
|                    E_NO_MATCH  - nothing found
+--------------------------------------------------------------------------*/
static int Match_Next_Character_In_Item_Name(MENU *menu, int ch, ITEM **item)
{
  bool found = FALSE, passed = FALSE;
  int  idx, last;
  
  assert( menu && item && *item);
  idx = (*item)->index;
  
  if (ch && ch!=BS)
    {
      /* if we become to long, we need no further checking : there can't be
	 a match ! */
      if ((menu->pindex+1) > menu->namelen) 
	RETURN(E_NO_MATCH);
      
      Add_Character_To_Pattern(menu,ch);
      /* we artificially position one item back, because in the do...while
	 loop we start with the next item. This means, that with a new
	 pattern search we always start the scan with the actual item. If
	 we do a NEXT_PATTERN oder PREV_PATTERN search, we start with the
	 one after or before the actual item. */
      if (--idx < 0) 
	idx = menu->nitems-1;
    }
  
  last = idx;			/* this closes the cycle */
  
  do{
    if (ch==BS)
      {			/* we have to go backward */
	if (--idx < 0) 
	  idx = menu->nitems-1;
      }
    else
      {			/* otherwise we always go forward */
	if (++idx >= menu->nitems) 
	  idx = 0;
      }
    if (Is_Sub_String((menu->opt & O_IGNORECASE) != 0,
		      menu->pattern,
		      menu->items[idx]->name.str)
	)
      found = TRUE;
    else
      passed = TRUE;    
  } while (!found && (idx != last));
  
  if (found)
    {
      if (!((idx==(*item)->index) && passed))
	{
	  *item = menu->items[idx];
	  RETURN(E_OK);
	}
      /* This point is reached, if we fully cycled through the item list
	 and the only match we found is the starting item. With a NEXT_PATTERN
	 or PREV_PATTERN scan this means, that there was no additional match.
	 If we searched with an expanded new pattern, we should never reach
	 this point, because if the expanded pattern matches also the actual
	 item we will find it in the first attempt (passed==FALSE) and we
	 will never cycle through the whole item array.   
	 */
      assert( ch==0 || ch==BS );
    }
  else
    {
      if (ch && ch!=BS && menu->pindex>0)
	{
	  /* if we had no match with a new pattern, we have to restore it */
	  Remove_Character_From_Pattern(menu);
	}
    }		
  RETURN(E_NO_MATCH);
}	

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
	  (Match_Next_Character_In_Item_Name(menu,*p,&matchitem) != E_OK) )
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

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int menu_driver(MENU *menu, int c)
|   
|   Description   :  Central dispatcher for the menu. Translates the logical
|                    request 'c' into a menu action.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid menu pointer
|                    E_BAD_STATE     - menu is in user hook routine
|                    E_NOT_POSTED    - menu is not posted
+--------------------------------------------------------------------------*/
int menu_driver(MENU * menu, int   c)
{
#define NAVIGATE(dir) \
  if (!item->dir)\
     result = E_REQUEST_DENIED;\
  else\
     item = item->dir

  int result = E_OK;
  ITEM *item;
  int my_top_row, rdiff;
  
  if (!menu)
    RETURN(E_BAD_ARGUMENT);
  
  if ( menu->status & _IN_DRIVER )
    RETURN(E_BAD_STATE);
  if ( !( menu->status & _POSTED ) )
    RETURN(E_NOT_POSTED);
  
  my_top_row = menu->toprow;
  item    = menu->curitem;
  assert(item);
  
  if ((c > KEY_MAX) && (c<=MAX_MENU_COMMAND))
    {  
      if (!((c==REQ_BACK_PATTERN)
	    || (c==REQ_NEXT_MATCH) || (c==REQ_PREV_MATCH)))
	{
	  assert( menu->pattern );
	  Reset_Pattern(menu);
	}
      
      switch(c)
	{
	case REQ_LEFT_ITEM:
	  /*=================*/  
	  NAVIGATE(left);
	  break;
	  
	case REQ_RIGHT_ITEM:
	  /*==================*/  
	  NAVIGATE(right);
	  break;
	  
	case REQ_UP_ITEM:
	  /*===============*/  
	  NAVIGATE(up);
	  break;
	  
	case REQ_DOWN_ITEM:
	  /*=================*/  
	  NAVIGATE(down);
	  break;
	  
	case REQ_SCR_ULINE:
	  /*=================*/  
	  if (my_top_row == 0)
	    result = E_REQUEST_DENIED;
	  else
	    {
	      --my_top_row;
	      item = item->up;
	    }  
	  break;
	  
	case REQ_SCR_DLINE:
	  /*=================*/  
	  my_top_row++;
	  if ((menu->rows - menu->height)>0)
	    {
	      /* only if the menu has less items than rows, we can deny the
		 request. Otherwise the epilogue of this routine adjusts the
		 top row if necessary */
	      my_top_row--;
	      result = E_REQUEST_DENIED;
	    }
	  else
	    item = item->down;
	  break;
	  
	case REQ_SCR_DPAGE:
	  /*=================*/  
	  rdiff = menu->rows - menu->height - my_top_row;
	  if (rdiff > menu->height) 
	    rdiff = menu->height;
	  if (rdiff==0)
	    result = E_REQUEST_DENIED;
	  else
	    {
	      my_top_row += rdiff;
	      while(rdiff-- > 0)
		item = item->down;
	    }
	  break;
	  
	case REQ_SCR_UPAGE:
	  /*=================*/  
	  rdiff = (menu->height < my_top_row) ?
	    menu->height : my_top_row;
	  if (rdiff==0)
	    result = E_REQUEST_DENIED;
	  else
	    {
	      my_top_row -= rdiff;
	      while(rdiff--)
		item = item->up;
	    }
	  break;
	  
	case REQ_FIRST_ITEM:
	  /*==================*/  
	  item = menu->items[0];
	  break;
	  
	case REQ_LAST_ITEM:
	  /*=================*/  
	  item = menu->items[menu->nitems-1];
	  break;

	case REQ_NEXT_ITEM:
	  /*=================*/  
	  if ((item->index+1)>=menu->nitems)
	    {
	      if (menu->opt & O_NONCYCLIC)
		result = E_REQUEST_DENIED;
	      else
		item = menu->items[0];
	    }
	  else
	    item = menu->items[item->index + 1];
	  break;
	  
	case REQ_PREV_ITEM:
	  /*=================*/  
	  if (item->index<=0)
	    {
	      if (menu->opt & O_NONCYCLIC)
		result = E_REQUEST_DENIED;
	      else
		item = menu->items[menu->nitems-1];
	    }
	  else
	    item = menu->items[item->index - 1];
	  break;
	  
	case REQ_TOGGLE_ITEM:
	  /*===================*/  
	  if (menu->opt & O_ONEVALUE)
	    {
	      result = E_REQUEST_DENIED;
	    }
	  else
	    {
	      if (menu->curitem->opt & O_SELECTABLE)
		{
		  menu->curitem->value = TRUE;
		  Move_And_Post_Item(menu,menu->curitem);
		  _nc_Show_Menu(menu);
		}
	      else
		result = E_NOT_SELECTABLE;
	    }
	  break;
	  
	case REQ_CLEAR_PATTERN:
	  /*=====================*/  
	  /* already cleared in prologue */
	  break;
	  
	case REQ_BACK_PATTERN:
	  /*====================*/  
	  if (menu->pindex>0)
	    {
	      assert(menu->pattern);
	      Remove_Character_From_Pattern(menu);
	      pos_menu_cursor( menu );
	    }
	  else
	    result = E_REQUEST_DENIED;
	  break;
	  
	case REQ_NEXT_MATCH:
	  /*==================*/  
	  assert(menu->pattern);
	  if (menu->pattern[0])
	    result = Match_Next_Character_In_Item_Name(menu,0,&item);
	  else
	    {
	      if ((item->index+1)<menu->nitems)
		item=menu->items[item->index+1];
	      else
		{
		  if (menu->opt & O_NONCYCLIC)
		    result = E_REQUEST_DENIED;
		  else
		    item = menu->items[0];
		}
	    }
	  break;	
	  
	case REQ_PREV_MATCH:
	  /*==================*/  
	  assert(menu->pattern);
	  if (menu->pattern[0])
	    result = Match_Next_Character_In_Item_Name(menu,BS,&item);
	  else
	    {
	      if (item->index)
		item = menu->items[item->index-1];
	      else
		{
		  if (menu->opt & O_NONCYCLIC)
		    result = E_REQUEST_DENIED;
		  else
		    item = menu->items[menu->nitems-1];
		}
	    }
	  break;
	  
	default:
	  /*======*/  
	  result = E_UNKNOWN_COMMAND;
	  break;
	}
    }
  else
    {				/* not a command */
      if ( !(c & ~((int)MAX_REGULAR_CHARACTER)) && isprint(c) )
	result = Match_Next_Character_In_Item_Name( menu, c, &item );
      else
	result = E_UNKNOWN_COMMAND;
    }
  
  /* Adjust the top row if it turns out that the current item unfortunately
     doesn't appear in the menu window */
  if ( item->y < my_top_row )
    my_top_row = item->y;
  else if ( item->y >= (my_top_row + menu->height) )
    my_top_row = item->y - menu->height + 1;
  
  _nc_New_TopRow_and_CurrentItem( menu, my_top_row, item );
  
  RETURN(result);
}

/* m_driver.c ends here */
