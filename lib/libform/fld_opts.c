/*	$OpenBSD: fld_opts.c,v 1.1 1997/12/03 05:39:56 millert Exp $	*/

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

MODULE_ID("Id: fld_opts.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*----------------------------------------------------------------------------
  Field-Options manipulation routines
  --------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_opts(FIELD *field, Field_Options opts)
|   
|   Description   :  Turns on the named options for this field and turns
|                    off all the remaining options.
|
|   Return Values :  E_OK            - success
|                    E_CURRENT       - the field is the current field
|                    E_BAD_ARGUMENT  - invalid options
|                    E_SYSTEM_ERROR  - system error
+--------------------------------------------------------------------------*/
int set_field_opts(FIELD * field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;
  if (!(opts & ~ALL_FIELD_OPTS))
    res = _nc_Synchronize_Options( Normalize_Field(field), opts );
  RETURN(res);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Field_Options field_opts(const FIELD *field)
|   
|   Description   :  Retrieve the fields options.
|
|   Return Values :  The options.
+--------------------------------------------------------------------------*/
Field_Options field_opts(const FIELD * field)
{
  return ALL_FIELD_OPTS & Normalize_Field( field )->opts;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_opts_on(FIELD *field, Field_Options opts)
|   
|   Description   :  Turns on the named options for this field and all the 
|                    remaining options are unchanged.
|
|   Return Values :  E_OK            - success
|                    E_CURRENT       - the field is the current field
|                    E_BAD_ARGUMENT  - invalid options
|                    E_SYSTEM_ERROR  - system error
+--------------------------------------------------------------------------*/
int field_opts_on(FIELD * field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;

  if (!(opts & ~ALL_FIELD_OPTS))
    {
      Normalize_Field( field );
      res = _nc_Synchronize_Options( field, field->opts | opts );
    }
  RETURN(res);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_opts_off(FIELD *field, Field_Options opts)
|   
|   Description   :  Turns off the named options for this field and all the 
|                    remaining options are unchanged.
|
|   Return Values :  E_OK            - success
|                    E_CURRENT       - the field is the current field
|                    E_BAD_ARGUMENT  - invalid options
|                    E_SYSTEM_ERROR  - system error
+--------------------------------------------------------------------------*/
int field_opts_off(FIELD  * field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;

  if (!(opts & ~ALL_FIELD_OPTS))
    {
      Normalize_Field( field );
      res = _nc_Synchronize_Options( field, field->opts & ~opts );
    }
  RETURN(res);
}	

/* fld_opts.c ends here */
