
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

/* "Template" macro to generate function to set application specific hook */
#define GEN_HOOK_SET_FUNCTION( typ, name ) \
int set_ ## typ ## _ ## name (FORM *form, Form_Hook func)\
{\
   (Normalize_Form( form ) -> typ ## name) = func ;\
   RETURN(E_OK);\
}

/* "Template" macro to generate function to get application specific hook */
#define GEN_HOOK_GET_FUNCTION( typ, name ) \
Form_Hook typ ## _ ## name ( const FORM *form )\
{\
   return ( Normalize_Form( form ) -> typ ## name );\
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_init(FORM *form, Form_Hook f)
|   
|   Description   :  Assigns an application defined initialization function
|                    to be called when the form is posted and just after
|                    the current field changes.
|
|   Return Values :  E_OK      - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION(field,init)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Form_Hook field_init(const FORM *form)
|   
|   Description   :  Retrieve field initialization routine address.
|
|   Return Values :  The address or NULL if no hook defined.
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION(field,init)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_term(FORM *form, Form_Hook f)
|   
|   Description   :  Assigns an application defined finalization function
|                    to be called when the form is unposted and just before
|                    the current field changes.
|
|   Return Values :  E_OK      - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION(field,term)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Form_Hook field_term(const FORM *form)
|   
|   Description   :  Retrieve field finalization routine address.
|
|   Return Values :  The address or NULL if no hook defined.
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION(field,term)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_init(FORM *form, Form_Hook f)
|   
|   Description   :  Assigns an application defined initialization function
|                    to be called when the form is posted and just after
|                    a page change.
|
|   Return Values :  E_OK       - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION(form,init)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Form_Hook form_init(const FORM *form)
|   
|   Description   :  Retrieve form initialization routine address.
|
|   Return Values :  The address or NULL if no hook defined.
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION(form,init)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_form_term(FORM *form, Form_Hook f)
|   
|   Description   :  Assigns an application defined finalization function
|                    to be called when the form is unposted and just before
|                    a page change.
|
|   Return Values :  E_OK       - success
+--------------------------------------------------------------------------*/
GEN_HOOK_SET_FUNCTION(form,term)

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  Form_Hook form_term(const FORM *form)
|   
|   Description   :  Retrieve form finalization routine address.
|
|   Return Values :  The address or NULL if no hook defined.
+--------------------------------------------------------------------------*/
GEN_HOOK_GET_FUNCTION(form,term)

/* frm_hook.c ends here */
