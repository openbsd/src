
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
 *	lib_tracedmp.c - Tracing/Debugging routines
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>

MODULE_ID("Id: lib_tracedmp.c,v 1.9 1997/01/15 00:39:27 tom Exp $")

void _tracedump(const char *name, WINDOW *win)
{
    int	i, j, n, width;

    /* compute narrowest possible display width */
    for (width = i = 0; i <= win->_maxy; i++)
    {
	n = 0;
	for (j = 0; j <= win->_maxx; j++)
	  if (win->_line[i].text[j] != ' ')
	    n = j;

	if (n > width)
	  width = n;
    }
    if (width < win->_maxx)
      ++width;

    for (n = 0; n <= win->_maxy; n++)
    {
	char	buf[BUFSIZ], *ep;
	bool haveattrs, havecolors;

	/* dump A_CHARTEXT part */
	(void) sprintf(buf, "%s[%2d] %3d%3d ='",
		name, n,
		win->_line[n].firstchar,
		win->_line[n].lastchar);
	ep = buf + strlen(buf);
	for (j = 0; j <= width; j++) {
	    ep[j] = TextOf(win->_line[n].text[j]);
	    if (ep[j] == 0)
	    	ep[j] = '.';
	}
	ep[j] = '\'';
	ep[j+1] = '\0';
	_tracef(buf);

	/* dump A_COLOR part, will screw up if there are more than 96 */
	havecolors = FALSE;
	for (j = 0; j <= width; j++)
	    if (win->_line[n].text[j] & A_COLOR)
	    {
		havecolors = TRUE;
		break;
	    }
	if (havecolors)
	{
	    (void) sprintf(buf, "%*s[%2d]%*s='", (int)strlen(name), "colors", n, 8, " ");
	    ep = buf + strlen(buf);
	    for (j = 0; j <= width; j++)
		ep[j] = ((win->_line[n].text[j] >> 8) & 0xff) + ' ';
	    ep[j] = '\'';
	    ep[j+1] = '\0';
	    _tracef(buf);
	}

	for (i = 0; i < 4; i++)
	{
	    const char	*hex = " 123456789ABCDEF";
	    chtype	mask = (0xf << ((i + 4) * 4));

	    haveattrs = FALSE;
	    for (j = 0; j <= width; j++)
		if (win->_line[n].text[j] & mask)
		{
		    haveattrs = TRUE;
		    break;
		}
	    if (haveattrs)
	    {
		(void) sprintf(buf, "%*s%d[%2d]%*s='", (int)strlen(name)-1, "attrs", i, n, 8, " ");
		ep = buf + strlen(buf);
		for (j = 0; j <= width; j++)
		    ep[j] = hex[(win->_line[n].text[j] & mask) >> ((i + 4) * 4)];
		ep[j] = '\'';
		ep[j+1] = '\0';
		_tracef(buf);
	    }
	}
    }
}
