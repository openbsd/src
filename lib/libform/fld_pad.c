/*	$OpenBSD: fld_pad.c,v 1.1 1997/12/03 05:39:56 millert Exp $	*/

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

MODULE_ID("Id: fld_pad.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_pad(FIELD *field, int ch)
|   
|   Description   :  Set the pad character used to fill the field. This must
|                    be a printable character.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid field pointer or pad character
|                    E_SYSTEM_ERROR - system error
+--------------------------------------------------------------------------*/
int set_field_pad(FIELD  * field, int ch)
{
  int res = E_BAD_ARGUMENT;

  Normalize_Field( field );
  if (isprint((unsigned char)ch))
    {
      if (field->pad != ch)
	{
	  field->pad = ch;
	  res = _nc_Synchronize_Attributes( field );
	}
      else
	res = E_OK;
    }
  RETURN(res);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_pad(const FIELD *field)
|   
|   Description   :  Retrieve the fields pad character.
|
|   Return Values :  The pad character.
+--------------------------------------------------------------------------*/
int field_pad(const FIELD * field)
{
  return Normalize_Field( field )->pad;
}

/* fld_pad.c ends here */
