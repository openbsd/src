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

#include "form.priv.h"

MODULE_ID("Id: frm_data.c,v 1.3 1997/05/01 16:47:54 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool data_behind(const FORM *form)
|   
|   Description   :  Check for off-screen data behind. This is nearly trivial
|                    becose the begin of a field is fixed.
|
|   Return Values :  TRUE   - there are off-screen data behind
|                    FALSE  - there are no off-screen data behind
+--------------------------------------------------------------------------*/
bool data_behind(const FORM *form)
{
  bool result = FALSE;

  if (form && (form->status & _POSTED) && form->current)
    {
      FIELD *field;

      field = form->current;
      if (!Single_Line_Field(field))
	{
	  result = (form->toprow==0) ? FALSE : TRUE;
	}
      else
	{
	  result = (form->begincol==0) ? FALSE : TRUE;
	}
    }
  return(result);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static char * After_Last_Non_Pad_Position(
|                                    char *buffer,
|                                    int len,
|                                    int pad)
|   
|   Description   :  Find the last position in the buffer that doesn't
|                    contain a padding character.
|
|   Return Values :  The pointer to this position 
+--------------------------------------------------------------------------*/
INLINE 
static char * After_Last_Non_Pad_Position(char *buffer, int len, int pad)
{
  char *end = buffer + len;

  assert(buffer && len>=0);
  while ( (buffer < end) && (*(end-1)==pad) )
    end--;

  return end;
}

#define SMALL_BUFFER_SIZE (80)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool data_ahead(const FORM *form)
|   
|   Description   :  Check for off-screen data ahead. This is more difficult
|                    because a dynamic field has a variable end. 
|
|   Return Values :  TRUE   - there are off-screen data ahead
|                    FALSE  - there are no off-screen data ahead
+--------------------------------------------------------------------------*/
bool data_ahead(const FORM *form)
{
  bool result = FALSE;

  if (form && (form->status & _POSTED) && form->current)
    {
      static char buffer[SMALL_BUFFER_SIZE + 1];
      FIELD *field;
      bool large_buffer;
      bool cursor_moved = FALSE;
      char *bp;
      char *found_content;
      int pos;

      field = form->current;
      assert(form->w);

      large_buffer = (field->cols > SMALL_BUFFER_SIZE);
      if (large_buffer)
	bp = (char *)malloc((size_t)(field->cols) + 1);
      else
	bp = buffer;

      assert(bp);
      
      if (Single_Line_Field(field))
	{
	  int check_len;

	  pos = form->begincol + field->cols;
	  while (pos < field->dcols)
	    {
	      check_len = field->dcols - pos;
	      if ( check_len >= field->cols )
		check_len = field->cols;
	      cursor_moved = TRUE;
	      wmove(form->w,0,pos);
	      winnstr(form->w,bp,check_len);
	      found_content = 
		After_Last_Non_Pad_Position(bp,check_len,field->pad);
	      if (found_content==bp)
		  pos += field->cols;		  
	      else
		{
		  result = TRUE;
		  break;
		}
	    }
	}
      else
	{
	  pos = form->toprow + field->rows;
	  while (pos < field->drows)
	    {
	      cursor_moved = TRUE;
	      wmove(form->w,pos,0);
	      pos++;
	      winnstr(form->w,bp,field->cols);
	      found_content = 
		After_Last_Non_Pad_Position(bp,field->cols,field->pad);
	      if (found_content!=bp)
		{
		  result = TRUE;
		  break;
		}
	    }
	}

      if (large_buffer)
	free(bp);

      if (cursor_moved)
	wmove(form->w,form->currow,form->curcol);
    }
  return(result);
}

/* frm_data.c ends here */
