/*	$OpenBSD: frm_post.c,v 1.1 1997/12/03 05:40:14 millert Exp $	*/

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

MODULE_ID("Id: frm_post.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int post_form(FORM * form)
|   
|   Description   :  Writes the form into its associated subwindow.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid form pointer
|                    E_POSTED          - form already posted
|                    E_NOT_CONNECTED   - no fields connected to form
|                    E_NO_ROOM         - form doesn't fit into subwindow
|                    E_SYSTEM_ERROR    - system error
+--------------------------------------------------------------------------*/
int post_form(FORM * form)
{
  WINDOW *formwin;
  int err;
  int page;

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (form->status & _POSTED)   
    RETURN(E_POSTED);

  if (!(form->field))
    RETURN(E_NOT_CONNECTED);
  
  formwin = Get_Form_Window(form);
  if ((form->cols > getmaxx(formwin)) || (form->rows > getmaxy(formwin))) 
    RETURN(E_NO_ROOM);

  /* reset form->curpage to an invald value. This forces Set_Form_Page
     to do the page initialization which is required by post_form.
  */
  page = form->curpage;
  form->curpage = -1;
  if ((err = _nc_Set_Form_Page(form,page,form->current))!=E_OK)
    RETURN(err);

  form->status |= _POSTED;

  Call_Hook(form,forminit);
  Call_Hook(form,fieldinit);

  _nc_Refresh_Current_Field(form);
  RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int unpost_form(FORM * form)
|   
|   Description   :  Erase form from its associated subwindow.
|
|   Return Values :  E_OK            - success
|                    E_BAD_ARGUMENT  - invalid form pointer
|                    E_NOT_POSTED    - form isn't posted
|                    E_BAD_STATE     - called from a hook routine
+--------------------------------------------------------------------------*/
int unpost_form(FORM * form)
{
  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!(form->status & _POSTED)) 
    RETURN(E_NOT_POSTED);

  if (form->status & _IN_DRIVER) 
    RETURN(E_BAD_STATE);

  Call_Hook(form,fieldterm);
  Call_Hook(form,formterm);

  werase(Get_Form_Window(form));
  delwin(form->w);
  form->w = (WINDOW *)0;
  form->status &= ~_POSTED;
  RETURN(E_OK);
}

/* frm_post.c ends here */
