/*	$OpenBSD: fld_just.c,v 1.1 1997/12/03 05:39:54 millert Exp $	*/

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

MODULE_ID("Id: fld_just.c,v 1.2 1997/10/26 11:20:59 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_just(FIELD *field, int just)
|   
|   Description   :  Set the fields type of justification.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - one of the arguments was incorrect
|                    E_SYSTEM_ERROR  - system error
+--------------------------------------------------------------------------*/
int set_field_just(FIELD * field, int just)
{
  int res = E_BAD_ARGUMENT;

  if ((just==NO_JUSTIFICATION)  ||
      (just==JUSTIFY_LEFT)	||
      (just==JUSTIFY_CENTER)	||
      (just==JUSTIFY_RIGHT)	)
    {
      Normalize_Field( field );
      if (field->just != just)
	{
	  field->just = just;
	  res = _nc_Synchronize_Attributes( field );
	}
      else
	res = E_OK;
    }
  RETURN(res);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_just( const FIELD *field )
|   
|   Description   :  Retrieve the fields type of justification
|
|   Return Values :  The justification type.
+--------------------------------------------------------------------------*/
int field_just(const FIELD * field)
{
  return Normalize_Field( field )->just;
}

/* fld_just.c ends here */
