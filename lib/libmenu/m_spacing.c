/*-----------------------------------------------------------------------------+
|           The ncurses menu library is  Copyright (C) 1995-1997               |
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

/***************************************************************************
* Module menu_spacing                                                      *
* Routines to handle spacing between entries                               *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("Id: m_spacing.c,v 1.7 1997/05/01 16:47:26 juergen Exp $")

#define MAX_SPC_DESC ((TABSIZE) ? (TABSIZE) : 8)
#define MAX_SPC_COLS ((TABSIZE) ? (TABSIZE) : 8)
#define MAX_SPC_ROWS (3)

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  int set_menu_spacing(MENU *menu,int desc, int r, int c);
|
|   Description   :  Set the spacing between entried
|
|   Return Values :  E_OK                 - on success
+--------------------------------------------------------------------------*/
int set_menu_spacing(MENU *menu, int s_desc, int s_row, int s_col )
{
  MENU *m; /* split for ATAC workaround */
  m = Normalize_Menu(menu);

  assert(m);
  if (m->status & _POSTED)
    RETURN(E_POSTED);

  if (((s_desc < 0) || (s_desc > MAX_SPC_DESC)) ||
      ((s_row  < 0) || (s_row  > MAX_SPC_ROWS)) ||
      ((s_col  < 0) || (s_col  > MAX_SPC_COLS)))
    RETURN(E_BAD_ARGUMENT);

  m->spc_desc = s_desc ? s_desc : 1;
  m->spc_rows = s_row  ? s_row  : 1;
  m->spc_cols = s_col  ? s_col  : 1;
  _nc_Calculate_Item_Length_and_Width(m);

  RETURN(E_OK);
}


/*---------------------------------------------------------------------------
|   Facility      :  libnmenu
|   Function      :  int menu_spacing (const MENU *,int *,int *,int *);
|
|   Description   :  Retrieve info about spacing between the entries
|
|   Return Values :  E_OK             - on success
+--------------------------------------------------------------------------*/
int menu_spacing( const MENU *menu, int* s_desc, int* s_row, int* s_col)
{
  const MENU *m; /* split for ATAC workaround */
  m = Normalize_Menu(menu);

  assert(m);
  if (s_desc) *s_desc = m->spc_desc;
  if (s_row)  *s_row  = m->spc_rows;
  if (s_col)  *s_col  = m->spc_cols;

  RETURN(E_OK);
}

/* m_spacing.c ends here */
