/*	$OpenBSD: screensize.c,v 1.1 1998/01/17 16:27:38 millert Exp $	*/

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


#include <curses.priv.h>

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>	/* lines, columns, cur_term */

#ifdef EXTERN_TERMINFO
void _ti_get_screensize(int *, int *, int *);
#endif

/****************************************************************************
 *
 * Terminal size computation
 *
 ****************************************************************************/

#if USE_SIZECHANGE
void _nc_update_screensize(void)
{
	int my_lines, my_cols;
#ifdef EXTERN_TERMINFO
	_ti_get_screensize(&my_lines, &my_cols, NULL);
#else
	_nc_get_screensize(&my_lines, &my_cols);
#endif
	if (SP != 0 && SP->_resize != 0)
		SP->_resize(my_lines, my_cols);
}
#endif
