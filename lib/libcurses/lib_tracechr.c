
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
 *	lib_tracechr.c - Tracing/Debugging routines
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>

#include <ctype.h>

char *_tracechar(const unsigned char ch)
{
    static char crep[20];
    /* 
     * We can show the actual character if it's either an ordinary printable
     * or one of the high-half characters.
     */
    if (isprint(ch) || (ch & 0x80))
    {
	crep[0] = '\'';
	crep[1] = ch;	/* necessary; printf tries too hard on metachars */
	(void) sprintf(crep + 2, "' = 0x%02x", (unsigned)ch);
    }
    else
	(void) sprintf(crep, "0x%02x", (unsigned)ch);
    return(crep);
}
