/*	$OpenBSD: frm_req_name.c,v 1.2 1997/12/03 05:40:14 millert Exp $	*/

/*-----------------------------------------------------------------------------+
|           The ncurses form library is  Copyright (C) 1995-1997               |
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
* Module form_request_name                                                 *
* Routines to handle external names of menu requests                       *
***************************************************************************/

#include "form.priv.h"

MODULE_ID("Id: frm_req_name.c,v 1.4 1997/05/01 16:47:54 juergen Exp $")

static const char *request_names[ MAX_FORM_COMMAND - MIN_FORM_COMMAND + 1 ] = {
  "NEXT_PAGE"	 ,
  "PREV_PAGE"	 ,
  "FIRST_PAGE"	 ,
  "LAST_PAGE"	 ,

  "NEXT_FIELD"	 ,
  "PREV_FIELD"	 ,
  "FIRST_FIELD"	 ,
  "LAST_FIELD"	 ,
  "SNEXT_FIELD"	 ,
  "SPREV_FIELD"	 ,
  "SFIRST_FIELD" ,
  "SLAST_FIELD"	 ,
  "LEFT_FIELD"	 ,
  "RIGHT_FIELD"	 ,
  "UP_FIELD"	 ,
  "DOWN_FIELD"	 ,

  "NEXT_CHAR"	 ,
  "PREV_CHAR"	 ,
  "NEXT_LINE"	 ,
  "PREV_LINE"	 ,
  "NEXT_WORD"	 ,
  "PREV_WORD"	 ,
  "BEG_FIELD"	 ,
  "END_FIELD"	 ,
  "BEG_LINE"	 ,
  "END_LINE"	 ,
  "LEFT_CHAR"	 ,
  "RIGHT_CHAR"	 ,
  "UP_CHAR"	 ,
  "DOWN_CHAR"	 ,

  "NEW_LINE"	 ,
  "INS_CHAR"	 ,
  "INS_LINE"	 ,
  "DEL_CHAR"	 ,
  "DEL_PREV"	 ,
  "DEL_LINE"	 ,
  "DEL_WORD"	 ,
  "CLR_EOL"	 ,
  "CLR_EOF"	 ,
  "CLR_FIELD"	 ,
  "OVL_MODE"	 ,
  "INS_MODE"	 ,
  "SCR_FLINE"	 ,
  "SCR_BLINE"	 ,
  "SCR_FPAGE"	 ,
  "SCR_BPAGE"	 ,
  "SCR_FHPAGE"   ,
  "SCR_BHPAGE"   ,
  "SCR_FCHAR"    ,
  "SCR_BCHAR"    ,
  "SCR_HFLINE"   ,
  "SCR_HBLINE"   ,
  "SCR_HFHALF"   ,
  "SCR_HBHALF"   ,

  "VALIDATION"	 ,
  "NEXT_CHOICE"	 ,
  "PREV_CHOICE"	 
};
#define A_SIZE (sizeof(request_names)/sizeof(request_names[0]))

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  const char * form_request_name (int request);
|   
|   Description   :  Get the external name of a form request.
|
|   Return Values :  Pointer to name      - on success
|                    NULL                 - on invalid request code
+--------------------------------------------------------------------------*/
const char *form_request_name( int request )
{
  if ( (request < MIN_FORM_COMMAND) || (request > MAX_FORM_COMMAND) )
    {
      SET_ERROR (E_BAD_ARGUMENT);
      return (const char *)0;
    }
  else
    return request_names[ request - MIN_FORM_COMMAND ];
}


/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int form_request_by_name (const char *str);
|   
|   Description   :  Search for a request with this name.
|
|   Return Values :  Request Id       - on success
|                    E_NO_MATCH       - request not found
+--------------------------------------------------------------------------*/
int form_request_by_name( const char *str )
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
	    return MIN_FORM_COMMAND + i;
	} 
    }
  RETURN(E_NO_MATCH);
}

/* frm_req_name.c ends here */
