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

MODULE_ID("Id: frm_win.c,v 1.4 1997/05/01 16:47:54 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_win(FORM *form,WINDOW *win)
|   
|   Description   :  Set the window of the form to win. 
|
|   Return Values :  E_OK       - success
|                    E_POSTED   - form is posted
+--------------------------------------------------------------------------*/
int set_form_win(FORM * form, WINDOW * win)
{
  if (form && (form->status & _POSTED))	
    RETURN(E_POSTED);

  Normalize_Form( form )->win = win;
  RETURN(E_OK);
}	

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  WINDOW *form_win(const FORM *)
|   
|   Description   :  Retrieve the window of the form.
|
|   Return Values :  The pointer to the Window or stdscr if there is none.
+--------------------------------------------------------------------------*/
WINDOW *form_win(const FORM * form)
{
  const FORM* f = Normalize_Form( form );
  return (f->win ? f->win : stdscr);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_sub(FORM *form, WINDOW *win)
|   
|   Description   :  Set the subwindow of the form to win. 
|
|   Return Values :  E_OK       - success
|                    E_POSTED   - form is posted
+--------------------------------------------------------------------------*/
int set_form_sub(FORM * form, WINDOW * win)
{
  if (form && (form->status & _POSTED))	
    RETURN(E_POSTED);

  Normalize_Form( form )->sub = win;
  RETURN(E_OK);
}	

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  WINDOW *form_sub(const FORM *)
|   
|   Description   :  Retrieve the window of the form.
|
|   Return Values :  The pointer to the Subwindow.
+--------------------------------------------------------------------------*/
WINDOW *form_sub(const FORM * form)
{
  const FORM* f = Normalize_Form( form );
  return Get_Form_Window(f);
}

/* frm_win.c ends here */
