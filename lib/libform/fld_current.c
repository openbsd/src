/*	$OpenBSD: fld_current.c,v 1.1 1997/12/03 05:39:51 millert Exp $	*/

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

MODULE_ID("Id: fld_current.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_current_field(FORM  * form,FIELD * field)
|   
|   Description   :  Set the current field of the form to the specified one.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid form or field pointer
|                    E_REQUEST_DENIED  - field not selectable
|                    E_BAD_STATE       - called from a hook routine
|                    E_INVALID_FIELD   - current field can't be left
|                    E_SYSTEM_ERROR    - system error
+--------------------------------------------------------------------------*/
int set_current_field(FORM  * form, FIELD * field)
{
  int err = E_OK;

  if ( !form || !field )
    RETURN(E_BAD_ARGUMENT);

  if ( (form != field->form) || Field_Is_Not_Selectable(field) )
    RETURN(E_REQUEST_DENIED);

  if (!(form->status & _POSTED))
    {
      form->current = field;
      form->curpage = field->page;
  }
  else
    {
      if (form->status & _IN_DRIVER) 
	err = E_BAD_STATE;
      else
	{
	  if (form->current != field)
	    {
	      if (!_nc_Internal_Validation(form)) 
	       err = E_INVALID_FIELD;
	      else
		{
		  Call_Hook(form,fieldterm);
		  if (field->page != form->curpage)
		    {
		      Call_Hook(form,formterm);
		      err = _nc_Set_Form_Page(form,field->page,field);
		      Call_Hook(form,forminit);
		    } 
		  else 
		    {
		      err = _nc_Set_Current_Field(form,field);
		    }
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
|   Function      :  FIELD *current_field(const FORM * form)
|   
|   Description   :  Return the current field.
|
|   Return Values :  Pointer to the current field.
+--------------------------------------------------------------------------*/
FIELD *current_field(const FORM * form)
{
  return Normalize_Form(form)->current;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int field_index(const FIELD * field)
|   
|   Description   :  Return the index of the field in the field-array of
|                    the form.
|
|   Return Values :  >= 0   : field index
|                    -1     : fieldpointer invalid or field not connected
+--------------------------------------------------------------------------*/
int field_index(const FIELD * field)
{
  return ( (field && field->form) ? field->index : -1 );
}

/* fld_current.c ends here */
