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

MODULE_ID("Id: frm_user.c,v 1.4 1997/05/01 16:47:54 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_userptr(FORM *form, void *usrptr)
|   
|   Description   :  Set the pointer that is reserved in any form to store
|                    application relevant informations
|
|   Return Values :  E_OK         - on success
+--------------------------------------------------------------------------*/
int set_form_userptr(FORM * form, const void *usrptr)
{
  Normalize_Form(form)->usrptr = usrptr;
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  void *form_userptr(const FORM *form)
|   
|   Description   :  Return the pointer that is reserved in any form to
|                    store application relevant informations.
|
|   Return Values :  Value of pointer. If no such pointer has been set,
|                    NULL is returned
+--------------------------------------------------------------------------*/
const void *form_userptr(const FORM * form)
{
  return Normalize_Form(form)->usrptr;
}

/* frm_user.c ends here */
