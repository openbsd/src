/*	$OpenBSD: frm_cursor.c,v 1.1 1997/12/03 05:40:10 millert Exp $	*/

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

MODULE_ID("Id: frm_cursor.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int pos_form_cursor(FORM * form)
|   
|   Description   :  Moves the form window cursor to the location required
|                    by the form driver to resume form processing. This may
|                    be needed after the application calls a curses library
|                    I/O routine that modifies the cursor position.
|
|   Return Values :  E_OK                      - Success
|                    E_SYSTEM_ERROR            - System error.
|                    E_BAD_ARGUMENT            - Invalid form pointer
|                    E_NOT_POSTED              - Form is not posted
+--------------------------------------------------------------------------*/
int pos_form_cursor(FORM * form)
{
  int res;

  if (!form)
   res = E_BAD_ARGUMENT;
  else
    {
      if (!(form->status & _POSTED))
        res = E_NOT_POSTED;
      else
	res = _nc_Position_Form_Cursor(form);
    }
  RETURN(res);
}

/* frm_cursor.c ends here */
