/*	$OpenBSD: fld_info.c,v 1.1 1997/12/03 05:39:53 millert Exp $	*/

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

MODULE_ID("Id: fld_info.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_info(const FIELD *field,
|                                   int *rows, int *cols,
|                                   int *frow, int *fcol,
|                                   int *nrow, int *nbuf)
|   
|   Description   :  Retrieve infos about the fields creation parameters.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid field pointer
+--------------------------------------------------------------------------*/
int field_info(const FIELD *field,
	       int *rows, int *cols, 
	       int *frow, int *fcol, 
	       int *nrow, int *nbuf)
{
  if (!field) 
    RETURN(E_BAD_ARGUMENT);

  if (rows) *rows = field->rows;
  if (cols) *cols = field->cols;
  if (frow) *frow = field->frow;
  if (fcol) *fcol = field->fcol;
  if (nrow) *nrow = field->nrow;
  if (nbuf) *nbuf = field->nbuf;
  RETURN(E_OK);
}
	
/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int dynamic_field_info(const FIELD *field,
|                                           int *drows, int *dcols,
|                                           int *maxgrow)
|   
|   Description   :  Retrieve informations about a dynamic fields current
|                    dynamic parameters.
|
|   Return Values :  E_OK           - success
|                    E_BAD_ARGUMENT - invalid argument
+--------------------------------------------------------------------------*/
int dynamic_field_info(const FIELD *field,
		       int *drows, int *dcols, int *maxgrow)
{
  if (!field)
    RETURN(E_BAD_ARGUMENT);

  if (drows)   *drows   = field->drows;
  if (dcols)   *dcols   = field->dcols;
  if (maxgrow) *maxgrow = field->maxgrow;

  RETURN(E_OK);
}

/* fld_info.c ends here */
