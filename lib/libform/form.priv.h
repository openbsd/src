
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

#include "mf_common.h"
#include "form.h"

/* form  status values */
#define _OVLMODE         (0x04) /* Form is in overlay mode                */
#define _WINDOW_MODIFIED (0x10) /* Current field window has been modified */
#define _FCHECK_REQUIRED (0x20) /* Current field needs validation         */

/* field status values */
#define _CHANGED         (0x01) /* Field has been changed                 */
#define _NEWTOP          (0x02) /* Vertical scrolling occured             */
#define _NEWPAGE	 (0x04) /* field begins new page of form          */
#define _MAY_GROW        (0x08) /* dynamic field may still grow           */

/* fieldtype status values */
#define _LINKED_TYPE     (0x01) /* Type is a linked type                  */
#define _HAS_ARGS        (0x02) /* Type has arguments                     */
#define _HAS_CHOICE      (0x04) /* Type has choice methods                */
#define _RESIDENT        (0x08) /* Type is builtin                        */

/* If form is NULL replace form argument by default-form */
#define Normalize_Form(form)  ((form)=(form)?(form):_nc_Default_Form)

/* If field is NULL replace field argument by default-field */
#define Normalize_Field(field)  ((field)=(field)?(field):_nc_Default_Field)

/* Retrieve forms window */
#define Get_Form_Window(form) \
  ((form)->sub?(form)->sub:((form)->win?(form)->win:stdscr))

/* Calculate the size for a single buffer for this field */
#define Buffer_Length(field) ((field)->drows * (field)->dcols)

/* Calculate the total size of all buffers for this field */
#define Total_Buffer_Size(field) \
   ( (Buffer_Length(field) + 1) * (1+(field)->nbuf) )

/* Logic to determine whether or not a field is single lined */
#define Single_Line_Field(field) \
   (((field)->rows + (field)->nrow) == 1)


typedef struct typearg {
  struct typearg *left;
  struct typearg *right;
} TypeArgument;

/* This is a dummy request code (normally invalid) to be used internally
   with the form_driver() routine to position to the first active field
   on the form
*/
#define FIRST_ACTIVE_MAGIC (-291056)

#define ALL_FORM_OPTS  (                \
			O_NL_OVERLOAD  |\
			O_BS_OVERLOAD   )

#define ALL_FIELD_OPTS (           \
			O_VISIBLE |\
			O_ACTIVE  |\
			O_PUBLIC  |\
			O_EDIT    |\
			O_WRAP    |\
			O_BLANK   |\
			O_AUTOSKIP|\
			O_NULLOK  |\
			O_PASSOK  |\
			O_STATIC   )


#define C_BLANK ' '
#define is_blank(c) ((c)==C_BLANK)
