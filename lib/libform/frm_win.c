
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

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
|   Return Values :  The pointer to the Window or NULL if there is none.
+--------------------------------------------------------------------------*/
WINDOW *form_win(const FORM * form)
{
  return Normalize_Form( form )->win;
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
|   Return Values :  The pointer to the Window or NULL if there is none.
+--------------------------------------------------------------------------*/
WINDOW *form_sub(const FORM * form)
{
  return Normalize_Form( form )->sub;
}

/* frm_win.c ends here */
