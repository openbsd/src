/*	$OpenBSD: fld_ftlink.c,v 1.1 1997/12/03 05:39:53 millert Exp $	*/

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

MODULE_ID("Id: fld_ftlink.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  FIELDTYPE *link_fieldtype(
|                                FIELDTYPE *type1,
|                                FIELDTYPE *type2)
|   
|   Description   :  Create a new fieldtype built from the two given types.
|                    They are connected by an logical 'OR'.
|                    If an error occurs, errno is set to                    
|                       E_BAD_ARGUMENT  - invalid arguments
|                       E_SYSTEM_ERROR  - system error (no memory)
|
|   Return Values :  Fieldtype pointer or NULL if error occured.
+--------------------------------------------------------------------------*/
FIELDTYPE *link_fieldtype(FIELDTYPE * type1, FIELDTYPE * type2)
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;

  if ( type1 && type2 )
    {
      nftyp = (FIELDTYPE *)malloc(sizeof(FIELDTYPE));
      if (nftyp)
	{
	  *nftyp = *_nc_Default_FieldType;
	  nftyp->status |= _LINKED_TYPE;
	  if ((type1->status & _HAS_ARGS) || (type2->status & _HAS_ARGS) )
	    nftyp->status |= _HAS_ARGS;
	  if ((type1->status & _HAS_CHOICE) || (type2->status & _HAS_CHOICE) )
	    nftyp->status |= _HAS_CHOICE;
	  nftyp->left  = type1;
	  nftyp->right = type2; 
	  type1->ref++;
	  type2->ref++;
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

/* fld_ftlink.c ends here */
