
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
* Module menu_post                                                         *
* Write or erase menus from associated subwindows                          *
***************************************************************************/

#include "menu.priv.h"

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void _nc_Post_Item(MENU *menu, ITEM *item)  
|   
|   Description   :  Draw the item in the menus window at the current
|                    window position 
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
void _nc_Post_Item(const MENU * menu, const ITEM * item)
{
  int i;
  chtype ch;
  bool isfore = FALSE, isback=FALSE, isgrey = FALSE;
  
  assert(menu->win);
  
  /* First we have to calculate the attribute depending on selectability
     and selection status
     */
  if (!(item->opt & O_SELECTABLE))
    {
      wattron(menu->win,menu->grey);
      isgrey = TRUE;
    }
  else
    {
      if (item->value || item==menu->curitem)
	{
	  wattron(menu->win,menu->fore);
	  isfore = TRUE;
	}
      else
	{
	  wattron(menu->win,menu->back);
	  isback = TRUE;
	}
    }
  
  /* We need a marker iff
     - it is a onevalued menu and it is the current item
     - or it has a selection value
     */
  if (item->value || ((menu->opt&O_ONEVALUE) && (item==menu->curitem))	)
    {
      if (menu->marklen) 
	waddstr(menu->win,menu->mark);
    }
  else			/* otherwise we have to wipe out the marker area */ 
    for(ch=menu->pad,i=menu->marklen;i>0;i--) 
      waddch(menu->win,ch);
  
  waddnstr(menu->win,item->name.str,item->name.length);
  for(ch=menu->pad,i=menu->namelen-item->name.length;i>0;i--)
    {
      waddch(menu->win,ch);
    }
  
  /* Show description if required and available */
  if ( (menu->opt & O_SHOWDESC) && menu->desclen>0 )
    {
      waddch(menu->win,menu->pad);
      if (item->description.length)
	waddnstr(menu->win,item->description.str,item->description.length);
      for(ch=menu->pad,i=menu->desclen-item->description.length; i>0; i--)
	{
	  waddch(menu->win,ch);
	}
    }
  
  /* Remove attributes */
  if (isfore)
    wattroff(menu->win,menu->fore);
  if (isback)
    wattroff(menu->win,menu->back);
  if (isgrey)
    wattroff(menu->win,menu->grey);
}	

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  void _nc_Draw_Menu(const MENU *)
|   
|   Description   :  Display the menu in its windows
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
void _nc_Draw_Menu(const MENU * menu)
{
  ITEM *item = menu->items[0];
  ITEM *lasthor, *lastvert;
  ITEM *hitem;
  int y = 0;
  
  assert(item && menu->win);
  
  lastvert = (menu->opt & O_NONCYCLIC) ? (ITEM *)0 : item;  
  
  do
    {  
      wmove(menu->win,y++,0);
      
      hitem   = item;
      lasthor = (menu->opt & O_NONCYCLIC) ? (ITEM *)0 : hitem;
      
      do
	{
	  _nc_Post_Item( menu, hitem);
	  if ( ((hitem = hitem->right) != lasthor) && hitem )
	    {
	      waddch( menu->win,menu->pad);
	    }
	} while (hitem && (hitem != lasthor));
      
      item = item->down;
      
    } while( item && (item != lastvert) );	
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int post_menu(MENU *)
|   
|   Description   :  Post a menu to the screen. This makes it visible.
|
|   Return Values :  E_OK                - success
|                    E_BAD_ARGUMENT      - not a valid menu pointer
|                    E_SYSTEM_ERROR      - error in lower layers
|                    E_NO_ROOM           - Menu to large for screen
|                    E_NOT_CONNECTED     - No items connected to menu
|                    E_BAD_STATE         - Menu in userexit routine
|                    E_POSTED            - Menu already posted
+--------------------------------------------------------------------------*/
int post_menu(MENU * menu)
{
  if (!menu)
    RETURN(E_BAD_ARGUMENT);
  
  if ( menu->status & _IN_DRIVER )
    RETURN(E_BAD_STATE);

  if ( menu->status & _POSTED )
    RETURN(E_POSTED);
  
  if (menu->items && *(menu->items))
    {
      int y;
      WINDOW *win = Get_Menu_Window(menu);
      int maxy = getmaxy(win);
      int maxx = getmaxx(win);
      
      if (maxx < menu->width || maxy < menu->height)
	RETURN(E_NO_ROOM);

      if ( (menu->win = newpad(menu->rows,menu->width)) )
	{
	  y = (maxy >= menu->rows) ? menu->rows : maxy;
	  if (y>=menu->height) 
	    y = menu->height;
	  if(!(menu->sub = subpad(menu->win,y,menu->width,0,0)))
	    RETURN(E_SYSTEM_ERROR);
	}
      else 
	RETURN(E_SYSTEM_ERROR);	
      
      if (menu->status & _LINK_NEEDED) 
	_nc_Link_Items(menu);
    }
  else
    RETURN(E_NOT_CONNECTED);
  
  menu->status |= _POSTED;

  if (!(menu->opt&O_ONEVALUE))
    {
      ITEM **items;
  
      for(items=menu->items;*items;items++)
	{
	  (*items)->value = FALSE;
	}
    }
  
  _nc_Draw_Menu(menu);
  
  Call_Hook(menu,menuinit);
  Call_Hook(menu,iteminit);
  
  _nc_Show_Menu(menu);
  
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int unpost_menu(MENU *)
|   
|   Description   :  Detach menu from screen
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - not a valid menu pointer
|                    E_BAD_STATE       - menu in userexit routine
|                    E_NOT_POSTED      - menu is not posted
+--------------------------------------------------------------------------*/
int unpost_menu(MENU * menu)
{
  WINDOW *win;
  
  if (!menu)
    RETURN(E_BAD_ARGUMENT);
  
  if ( menu->status & _IN_DRIVER )
    RETURN(E_BAD_STATE);

  if ( !( menu->status & _POSTED ) )
    RETURN(E_NOT_POSTED);
  
  Call_Hook(menu,itemterm);
  Call_Hook(menu,menuterm);	
  
  win = Get_Menu_Window(menu);
  werase(win);
  wsyncup(win);
  
  assert(menu->sub);
  delwin(menu->sub);
  menu->sub = (WINDOW *)0;
  
  assert(menu->win);
  delwin(menu->win);
  menu->win = (WINDOW *)0;
  
  menu->status &= ~_POSTED;
  
  RETURN(E_OK);
}

/* m_post.c ends here */
