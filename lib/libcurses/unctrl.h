/*	$OpenBSD: unctrl.h,v 1.3 1997/12/03 05:21:45 millert Exp $	*/

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

/*
 * unctrl.h
 *
 * Display a printable version of a control character.
 * Control characters are displayed in caret notation (^x), DELETE is displayed
 * as ^?. Printable characters are displayed as is.
 */

/* Id: unctrl.h.in,v 1.7 1997/09/13 23:19:40 tom Exp $ */

#ifndef _UNCTRL_H
#define _UNCTRL_H	1

#undef  NCURSES_VERSION
#define NCURSES_VERSION "4.1"

#ifdef __cplusplus
extern "C" {
#endif

#include <curses.h>

#undef unctrl
extern NCURSES_CONST char *unctrl(chtype);

#ifdef __cplusplus
}
#endif

#endif /* _UNCTRL_H */
