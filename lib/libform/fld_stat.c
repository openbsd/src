
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
|   Function      :  int set_field_status(FIELD *field, bool status)
|   
|   Description   :  Set or clear the 'changed' indication flag for that
|                    fields primary buffer.
|
|   Return Values :  E_OK            - success
+--------------------------------------------------------------------------*/
int set_field_status(FIELD * field, bool status)
{
  Normalize_Field( field );

  if (status)
    field->status |= _CHANGED;
  else
    field->status &= ~_CHANGED;

  return(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  bool field_status(const FIELD *field)
|   
|   Description   :  Retrieve the value of the 'changed' indication flag
|                    for that fields primary buffer. 
|
|   Return Values :  TRUE  - buffer has been changed
|                    FALSE - buffer has not been changed
+--------------------------------------------------------------------------*/
bool field_status(const FIELD * field)
{
  return ((Normalize_Field(field)->status & _CHANGED) ? TRUE : FALSE);
}

/* fld_stat.c ends here */
