/*	$OpenBSD: frm_page.c,v 1.1 1997/12/03 05:40:14 millert Exp $	*/

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

MODULE_ID("Id: frm_page.c,v 1.2 1997/10/26 11:21:04 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_page(FORM * form,int  page)
|   
|   Description   :  Set the page number of the form.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid form pointer or page number
|                    E_BAD_STATE       - called from a hook routine
|                    E_INVALID_FIELD   - current field can't be left
|                    E_SYSTEM_ERROR    - system error
+--------------------------------------------------------------------------*/
int set_form_page(FORM * form, int page)
{
  int err = E_OK;

  if ( !form || (page<0) || (page>=form->maxpage) )
    RETURN(E_BAD_ARGUMENT);

  if (!(form->status & _POSTED))
    {
      form->curpage = page;
      form->current = _nc_First_Active_Field(form);
  }
  else
    {
      if (form->status & _IN_DRIVER) 
	err = E_BAD_STATE;
      else
	{
	  if (form->curpage != page)
	    {
	      if (!_nc_Internal_Validation(form)) 
		err = E_INVALID_FIELD;
	      else
		{
		  Call_Hook(form,fieldterm);
		  Call_Hook(form,formterm);
		  err = _nc_Set_Form_Page(form,page,(FIELD *)0);
		  Call_Hook(form,forminit);
		  Call_Hook(form,fieldinit);
		  _nc_Refresh_Current_Field(form);
		}
	    }
	}
    }
  RETURN(err);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int form_page(const FORM * form)
|   
|   Description   :  Return the current page of the form.
|
|   Return Values :  >= 0  : current page number
|                    -1    : invalid form pointer
+--------------------------------------------------------------------------*/
int form_page(const FORM * form)
{
  return Normalize_Form(form)->curpage;
}

/* frm_page.c ends here */
