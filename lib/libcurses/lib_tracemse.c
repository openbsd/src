
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
 *	lib_tracemse.c - Tracing/Debugging routines (mouse events)
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>

MODULE_ID("Id: lib_tracemse.c,v 1.4 1996/12/21 14:24:06 tom Exp $")

char *_tracemouse(MEVENT const *ep)
{
	static char buf[80];

	(void) sprintf(buf, "id %2d  at (%2d, %2d, %2d) state %4lx = {",
		       ep->id, ep->x, ep->y, ep->z, ep->bstate);

#define SHOW(m, s) if ((ep->bstate & m)==m) {strcat(buf,s); strcat(buf, ", ");}
	SHOW(BUTTON1_RELEASED,		"release-1")
	SHOW(BUTTON1_PRESSED,		"press-1")
	SHOW(BUTTON1_CLICKED,		"click-1")
	SHOW(BUTTON1_DOUBLE_CLICKED,	"doubleclick-1")
	SHOW(BUTTON1_TRIPLE_CLICKED,	"tripleclick-1")
	SHOW(BUTTON1_RESERVED_EVENT,	"reserved-1")
	SHOW(BUTTON2_RELEASED,		"release-2")
	SHOW(BUTTON2_PRESSED,		"press-2")
	SHOW(BUTTON2_CLICKED,		"click-2")
	SHOW(BUTTON2_DOUBLE_CLICKED,	"doubleclick-2")
	SHOW(BUTTON2_TRIPLE_CLICKED,	"tripleclick-2")
	SHOW(BUTTON2_RESERVED_EVENT,	"reserved-2")
	SHOW(BUTTON3_RELEASED,		"release-3")
	SHOW(BUTTON3_PRESSED,		"press-3")
	SHOW(BUTTON3_CLICKED,		"click-3")
	SHOW(BUTTON3_DOUBLE_CLICKED,	"doubleclick-3")
	SHOW(BUTTON3_TRIPLE_CLICKED,	"tripleclick-3")
	SHOW(BUTTON3_RESERVED_EVENT,	"reserved-3")
	SHOW(BUTTON4_RELEASED,		"release-4")
	SHOW(BUTTON4_PRESSED,		"press-4")
	SHOW(BUTTON4_CLICKED,		"click-4")
	SHOW(BUTTON4_DOUBLE_CLICKED,	"doubleclick-4")
	SHOW(BUTTON4_TRIPLE_CLICKED,	"tripleclick-4")
	SHOW(BUTTON4_RESERVED_EVENT,	"reserved-4")
	SHOW(BUTTON_CTRL,		"ctrl")
	SHOW(BUTTON_SHIFT,		"shift")
	SHOW(BUTTON_ALT,		"alt")
	SHOW(ALL_MOUSE_EVENTS,		"all-events")
	SHOW(REPORT_MOUSE_POSITION,	"position")
#undef SHOW

	if (buf[strlen(buf)-1] == ' ')
		buf[strlen(buf)-2] = '\0';
	(void) strcat(buf, "}");
	return(buf);
}



