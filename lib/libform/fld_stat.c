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

MODULE_ID("Id: fld_stat.c,v 1.3 1997/05/01 16:47:54 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_status(FIELD *field, bool status)
|   
|   Description   :  Set or clear the 'changed' indication flag for that
|                    fields primary buffer.
|
|   Return Values :  E_OK            - success
+--------------------------------------------------------------------------*/
int set_field_status(FIELD * field, bool status)
{
  Normalize_Field( field );

  if (status)
    field->status |= _CHANGED;
  else
    field->status &= ~_CHANGED;

  return(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool field_status(const FIELD *field)
|   
|   Description   :  Retrieve the value of the 'changed' indication flag
|                    for that fields primary buffer. 
|
|   Return Values :  TRUE  - buffer has been changed
|                    FALSE - buffer has not been changed
+--------------------------------------------------------------------------*/
bool field_status(const FIELD * field)
{
  return ((Normalize_Field(field)->status & _CHANGED) ? TRUE : FALSE);
}

/* fld_stat.c ends here */
