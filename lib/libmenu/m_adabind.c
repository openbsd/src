/*	$OpenBSD: m_adabind.c,v 1.1 1998/07/24 16:38:52 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <Juergen.Pfeifer@T-Online.de> 1995,1997        *
 ****************************************************************************/

/***************************************************************************
* Module m_adabind.c                                                       *
* Helper routines to ease the implementation of an Ada95 binding to        *
* ncurses. For details and copyright of the binding see the ../Ada95       *
* subdirectory.                                                            *
***************************************************************************/
#include "menu.priv.h"

MODULE_ID("$From: m_adabind.c,v 1.6 1998/02/11 12:13:50 tom Exp $")

/* Prototypes for the functions in this module */
void  _nc_ada_normalize_menu_opts (int *opt);
void  _nc_ada_normalize_item_opts (int *opt);
ITEM* _nc_get_item(const MENU*, int);

void _nc_ada_normalize_menu_opts (int *opt)
{
  *opt = ALL_MENU_OPTS & (*opt);
}

void _nc_ada_normalize_item_opts (int *opt)
{
  *opt = ALL_ITEM_OPTS & (*opt);
}

ITEM* _nc_get_item(const MENU* menu, int idx) {
  if (menu && menu->items && idx>=0 && (idx<menu->nitems))
    {
      return menu->items[idx];
    }
  else
    return (ITEM*)0;
}
