
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
|   Function      :  int set_form_opts(FORM *form, Form_Options opts)
|   
|   Description   :  Turns on the named options and turns off all the
|                    remaining options for that form.
|
|   Return Values :  E_OK              - success
|                    E_BAD_ARGUMENT    - invalid options
+--------------------------------------------------------------------------*/
int set_form_opts(FORM * form, Form_Options  opts)
{
  if (opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form( form )->opts = opts;
      RETURN(E_OK);
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Form_Options form_opts(const FORM *)
|   
|   Description   :  Retrieves the current form options.
|
|   Return Values :  The option flags.
+--------------------------------------------------------------------------*/
Form_Options form_opts(const FORM * form)
{
  return (Normalize_Form(form)->opts & ALL_FORM_OPTS);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int form_opts_on(FORM *form, Form_Options opts)
|   
|   Description   :  Turns on the named options; no other options are 
|                    changed.
|
|   Return Values :  E_OK            - success 
|                    E_BAD_ARGUMENT  - invalid options
+--------------------------------------------------------------------------*/
int form_opts_on(FORM * form, Form_Options opts)
{
  if (opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form( form )->opts |= opts;	
      RETURN(E_OK);
    }
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int form_opts_off(FORM *form, Form_Options opts)
|   
|   Description   :  Turns off the named options; no other options are 
|                    changed.
|
|   Return Values :  E_OK            - success 
|                    E_BAD_ARGUMENT  - invalid options
+--------------------------------------------------------------------------*/
int form_opts_off(FORM * form, Form_Options opts)
{
  if (opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form(form)->opts &= ~opts;
      RETURN(E_OK);
    }
}

/* frm_opts.c ends here */
