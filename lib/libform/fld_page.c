/*	$OpenBSD: fld_page.c,v 1.1 1997/12/03 05:39:57 millert Exp $	*/

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

MODULE_ID("Id: fld_page.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_new_page(FIELD *field, bool new_page_flag)
|   
|   Description   :  Marks the field as the beginning of a new page of 
|                    the form.
|
|   Return Values :  E_OK         - success
|                    E_CONNECTED  - field is connected
+--------------------------------------------------------------------------*/
int set_new_page(FIELD * field, bool new_page_flag)
{
  Normalize_Field(field);
  if (field->form) 
    RETURN(E_CONNECTED);

  if (new_page_flag) 
    field->status |= _NEWPAGE;
  else
    field->status &= ~_NEWPAGE;

  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool new_page(const FIELD *field)
|   
|   Description   :  Retrieve the info whether or not the field starts a
|                    new page on the form.
|
|   Return Values :  TRUE  - field starts a new page
|                    FALSE - field doesn't start a new page
+--------------------------------------------------------------------------*/
bool new_page(const FIELD * field)
{
  return (Normalize_Field(field)->status & _NEWPAGE)  ? TRUE : FALSE;
}

/* fld_page.c ends here */
