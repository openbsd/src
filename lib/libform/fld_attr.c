/*	$OpenBSD: fld_attr.c,v 1.1 1997/12/03 05:39:51 millert Exp $	*/

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

MODULE_ID("Id: fld_attr.c,v 1.1 1997/10/21 13:24:19 juergen Exp $")

/*----------------------------------------------------------------------------
  Field-Attribute manipulation routines
  --------------------------------------------------------------------------*/
/* "Template" macro to generate a function to set a fields attribute */
#define GEN_FIELD_ATTR_SET_FCT( name ) \
int set_field_ ## name (FIELD * field, chtype attr)\
{\
   int res = E_BAD_ARGUMENT;\
   if ( attr==A_NORMAL || ((attr & A_ATTRIBUTES)==attr) )\
     {\
       Normalize_Field( field );\
       if ((field -> name) != attr)\
         {\
           field -> name = attr;\
           res = _nc_Synchronize_Attributes( field );\
         }\
       else\
	 res = E_OK;\
     }\
   RETURN(res);\
}

/* "Template" macro to generate a function to get a fields attribute */
#define GEN_FIELD_ATTR_GET_FCT( name ) \
chtype field_ ## name (const FIELD * field)\
{\
   return ( A_ATTRIBUTES & (Normalize_Field( field ) -> name) );\
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_fore(FIELD *field, chtype attr)
|   
|   Description   :  Sets the foreground of the field used to display the
|                    field contents.
|
|   Return Values :  E_OK             - success
|                    E_BAD_ARGUMENT   - invalid attributes
|                    E_SYSTEM_ERROR   - system error
+--------------------------------------------------------------------------*/
GEN_FIELD_ATTR_SET_FCT( fore )

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  chtype field_fore(const FIELD *)
|   
|   Description   :  Retrieve fields foreground attribute
|
|   Return Values :  The foreground attribute
+--------------------------------------------------------------------------*/
GEN_FIELD_ATTR_GET_FCT( fore )

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  int set_field_back(FIELD *field, chtype attr)
|   
|   Description   :  Sets the background of the field used to display the
|                    fields extend.
|
|   Return Values :  E_OK             - success
|                    E_BAD_ARGUMENT   - invalid attributes
|                    E_SYSTEM_ERROR   - system error
+--------------------------------------------------------------------------*/
GEN_FIELD_ATTR_SET_FCT( back )

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  chtype field_back(const 
|   
|   Description   :  Retrieve fields background attribute
|
|   Return Values :  The background attribute
+--------------------------------------------------------------------------*/
GEN_FIELD_ATTR_GET_FCT( back )

/* fld_attr.c ends here */
