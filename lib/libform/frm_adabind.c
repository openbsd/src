/*	$OpenBSD: frm_adabind.c,v 1.1 1997/12/03 05:40:10 millert Exp $	*/

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

/***************************************************************************
* Module frm_adabind.c                                                     *
* Helper routines to ease the implementation of an Ada95 binding to        *
* ncurses. For details and copyright of the binding see the ../Ada95       *
* subdirectory.                                                            *
***************************************************************************/
#include "form.priv.h"

MODULE_ID("Id: frm_adabind.c,v 1.4 1997/09/05 23:04:11 juergen Exp $")

/* Prototypes for the functions in this module */
void   _nc_ada_normalize_field_opts (int *opt);
void   _nc_ada_normalize_form_opts (int *opt);
void*  _nc_ada_getvarg(va_list *);
FIELD* _nc_get_field(const FORM*, int);


void _nc_ada_normalize_field_opts (int *opt)
{
  *opt = ALL_FIELD_OPTS & (*opt);
}

void _nc_ada_normalize_form_opts (int *opt)
{
  *opt = ALL_FORM_OPTS & (*opt);
}


/*  This tiny stub helps us to get a void pointer from an argument list.
//  The mechanism for libform to handle arguments to field types uses
//  unfortunately functions with variable argument lists. In the Ada95
//  binding we replace this by a mechanism that only uses one argument
//  that is a pointer to a record describing all the specifics of an
//  user defined field type. So we need only this simple generic
//  procedure to get the pointer from the arglist.
*/
void *_nc_ada_getvarg(va_list *ap)
{
  return va_arg(*ap,void*);
}

FIELD* _nc_get_field(const FORM* frm, int idx) {
  if (frm && frm->field && idx>=0 && (idx<frm->maxfield))
    {
      return frm->field[idx];
    }
  else
    return (FIELD*)0;
}
