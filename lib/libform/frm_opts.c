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

MODULE_ID("Id: frm_opts.c,v 1.3 1997/05/01 16:47:54 juergen Exp $")

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
