
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
**	lib_set_term.c
**
**	The routine set_term().
**
*/

#include "curses.priv.h"
#include <stdlib.h>
#include "term.h"	/* cur_term */

struct screen * set_term(struct screen *screen)
{
struct screen	*oldSP;

	T(("set_term(%p) called", screen));

	oldSP = SP;
	_nc_set_screen(screen);

	cur_term = SP->_term;
	curscr   = SP->_curscr;
	newscr   = SP->_newscr;
	stdscr   = SP->_stdscr;

	return(oldSP);
}

void delscreen(SCREEN *sp)
{
	free(sp);
}

struct ripoff_t
{
	int	line;
	int	(*hook)(WINDOW *, int);
}
rippedoff[5], *rsp = rippedoff;

int _nc_setupscreen(short slines, short const scolumns)
/* OS-independent screen initializations */
{
int	stolen, topstolen;

	if (!_nc_alloc_screen())
	    	return ERR;

	SP->_term      	= cur_term;
	SP->_lines	= slines;
	SP->_columns	= scolumns;
	SP->_cursrow   	= -1;
	SP->_curscol   	= -1;
	SP->_keytry    	= UNINITIALISED;
	SP->_nl        	= TRUE;
	SP->_raw       	= FALSE;
	SP->_cbreak    	= FALSE;
	SP->_echo      	= TRUE;
	SP->_fifohead	= -1;
	SP->_fifotail 	= 0;
	SP->_fifopeek	= 0;
	SP->_endwin	= TRUE;
	SP->_ofp	= stdout;	/* (may be overridden later) */
	SP->_coloron	= 0;

	init_acs(); 

	T(("creating newscr"));
	if ((newscr = newwin(slines, scolumns, 0, 0)) == (WINDOW *)NULL)
	    	return ERR;

	T(("creating curscr"));
	if ((curscr = newwin(slines, scolumns, 0, 0)) == (WINDOW *)NULL)
	    	return ERR;

	SP->_newscr = newscr;
	SP->_curscr = curscr;

	newscr->_clear = TRUE;
	curscr->_clear = FALSE;

	stolen = topstolen = 0;
	for (rsp = rippedoff; rsp->line; rsp++) {
		if (rsp->hook)
			if (rsp->line < 0)
				rsp->hook(newwin(1,scolumns, slines-1,0), scolumns);
			else
				rsp->hook(newwin(1,scolumns, topstolen++,0), scolumns);
		--slines;
		stolen++;
	}

	T(("creating stdscr"));
    	if ((stdscr = newwin(LINES = slines, scolumns, topstolen, 0)) == NULL)
		return ERR;
	SP->_stdscr = stdscr;

	def_shell_mode();
	def_prog_mode();

	return OK;
}

int
ripoffline(int line, int (*init)(WINDOW *, int))
{
    if (line == 0)
	return(OK);

    if (rsp >= rippedoff + sizeof(rippedoff)/sizeof(rippedoff[0]))
	return(ERR);

    rsp->line = line;
    rsp->hook = init;
    rsp++;

    return(OK);
}
