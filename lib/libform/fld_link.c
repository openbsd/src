/*	$OpenBSD: fld_link.c,v 1.1 1997/12/03 05:39:54 millert Exp $	*/

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

MODULE_ID("Id: fld_link.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELD *link_field(FIELD *field, int frow, int fcol)  
|   
|   Description   :  Duplicates the field at the specified position. The
|                    new field shares its buffers with the original one,
|                    the attributes are independent.
|                    If an error occurs, errno is set to
|                    
|                    E_BAD_ARGUMENT - invalid argument
|                    E_SYSTEM_ERROR - system error
|
|   Return Values :  Pointer to the new field or NULL if failure
+--------------------------------------------------------------------------*/
FIELD *link_field(FIELD * field, int frow, int fcol)
{
  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  if (field && (frow>=0) && (fcol>=0) &&
      ((err=E_SYSTEM_ERROR) != 0) && /* trick: this resets the default error */
      (New_Field = (FIELD *)malloc(sizeof(FIELD))) )
    {
      *New_Field        = *_nc_Default_Field;
      New_Field->frow   = frow;
      New_Field->fcol   = fcol;
      New_Field->link   = field->link;
      field->link       = New_Field;
      New_Field->buf    = field->buf;
      New_Field->rows   = field->rows;
      New_Field->cols   = field->cols;
      New_Field->nrow   = field->nrow;
      New_Field->nbuf   = field->nbuf;
      New_Field->drows  = field->drows;
      New_Field->dcols  = field->dcols;
      New_Field->maxgrow= field->maxgrow;
      New_Field->just   = field->just;
      New_Field->fore   = field->fore;
      New_Field->back   = field->back;
      New_Field->pad    = field->pad;
      New_Field->opts   = field->opts;
      New_Field->usrptr = field->usrptr;
      if (_nc_Copy_Type(New_Field,field)) 
	return New_Field;
    }

  if (New_Field) 
    free_field(New_Field);

  SET_ERROR( err );
  return (FIELD *)0;
}

/* fld_link.c ends here */
