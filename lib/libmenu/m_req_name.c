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
* Module menu_request_name                                                 *
* Routines to handle external names of menu requests                       *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_req_name.c,v 1.8 1997/05/01 16:47:26 juergen Exp $")

static const char *request_names[ MAX_MENU_COMMAND - MIN_MENU_COMMAND + 1 ] = {
  "LEFT_ITEM"    ,
  "RIGHT_ITEM"   ,
  "UP_ITEM"      ,
  "DOWN_ITEM"    ,
  "SCR_ULINE"    ,
  "SCR_DLINE"    ,
  "SCR_DPAGE"    ,
  "SCR_UPAGE"    ,
  "FIRST_ITEM"   ,
  "LAST_ITEM"    ,
  "NEXT_ITEM"    ,
  "PREV_ITEM"    ,
  "TOGGLE_ITEM"  ,
  "CLEAR_PATTERN",
  "BACK_PATTERN" ,
  "NEXT_MATCH"   ,
  "PREV_MATCH"          
};
#define A_SIZE (sizeof(request_names)/sizeof(request_names[0]))

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  const char * menu_request_name (int request);
|   
|   Description   :  Get the external name of a menu request.
|
|   Return Values :  Pointer to name      - on success
|                    NULL                 - on invalid request code
+--------------------------------------------------------------------------*/
const char *menu_request_name( int request )
{
  if ( (request < MIN_MENU_COMMAND) || (request > MAX_MENU_COMMAND) )
    {
      SET_ERROR(E_BAD_ARGUMENT);
      return (const char *)0;
    }
  else
    return request_names[ request - MIN_MENU_COMMAND ];
}


/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int menu_request_by_name (const char *str);
|   
|   Description   :  Search for a request with this name.
|
|   Return Values :  Request Id       - on success
|                    E_NO_MATCH       - request not found
+--------------------------------------------------------------------------*/
int menu_request_by_name( const char *str )
{ 
  /* because the table is so small, it doesn't really hurt
     to run sequentially through it.
  */
  unsigned int i = 0;
  char buf[16];
  
  if (str)
    {
      strncpy(buf,str,sizeof(buf));
      while( (i<sizeof(buf)) && (buf[i] != '\0') )
	{
	  buf[i] = toupper(buf[i]);
	  i++;
	}
      
      for (i=0; i < A_SIZE; i++)
	{
	  if (strncmp(request_names[i],buf,sizeof(buf))==0)
	    return MIN_MENU_COMMAND + i;
	} 
    }
  RETURN(E_NO_MATCH);
}

/* m_req_name.c ends here */
