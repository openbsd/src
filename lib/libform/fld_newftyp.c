/*	$OpenBSD: fld_newftyp.c,v 1.1 1997/12/03 05:39:56 millert Exp $	*/

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

MODULE_ID("Id: fld_newftyp.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

static FIELDTYPE const default_fieldtype = {
  0,                   /* status                                      */
  0L,                  /* reference count                             */
  (FIELDTYPE *)0,      /* pointer to left  operand                    */
  (FIELDTYPE *)0,      /* pointer to right operand                    */
  NULL,                /* makearg function                            */
  NULL,                /* copyarg function                            */
  NULL,                /* freearg function                            */
  NULL,                /* field validation function                   */
  NULL,                /* Character check function                    */
  NULL,                /* enumerate next function                     */
  NULL                 /* enumerate previous function                 */
};

const FIELDTYPE* _nc_Default_FieldType = &default_fieldtype;

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELDTYPE *new_fieldtype(
|                       bool (* const field_check)(FIELD *,const void *),
|                       bool (* const char_check) (int, const void *) ) 
|   
|   Description   :  Create a new fieldtype. The application programmer must
|                    write a field_check and a char_check function and give
|                    them as input to this call.
|                    If an error occurs, errno is set to                    
|                       E_BAD_ARGUMENT  - invalid arguments
|                       E_SYSTEM_ERROR  - system error (no memory)
|
|   Return Values :  Fieldtype pointer or NULL if error occured
+--------------------------------------------------------------------------*/
FIELDTYPE *new_fieldtype(
 bool (* const field_check)(FIELD *,const void *),
 bool (* const char_check) (int,const void *) )
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;
  
  if ( (field_check) && (char_check) )
    {
      nftyp = (FIELDTYPE *)malloc(sizeof(FIELDTYPE));
      if (nftyp)
	{
	  *nftyp = default_fieldtype;
	  nftyp->fcheck = field_check;
	  nftyp->ccheck = char_check;
	}
      else
	{
	  SET_ERROR( E_SYSTEM_ERROR );
	}
    }
  else
    {
      SET_ERROR( E_BAD_ARGUMENT );
    }
  return nftyp;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int free_fieldtype(FIELDTYPE *typ)
|   
|   Description   :  Release the memory associated with this fieldtype.
|
|   Return Values :  E_OK            - success
|                    E_CONNECTED     - there are fields referencing the type
|                    E_BAD_ARGUMENT  - invalid fieldtype pointer
+--------------------------------------------------------------------------*/
int free_fieldtype(FIELDTYPE *typ)
{
  if (!typ)
    RETURN(E_BAD_ARGUMENT);

  if (typ->ref!=0)
    RETURN(E_CONNECTED);

  if (typ->status & _RESIDENT)
    RETURN(E_CONNECTED);

  if (typ->status & _LINKED_TYPE)
    {
      if (typ->left ) typ->left->ref--;
      if (typ->right) typ->right->ref--;
    }
  free(typ);
  RETURN(E_OK);
}

/* fld_newftyp.c ends here */
