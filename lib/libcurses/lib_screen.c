
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


#include "curses.priv.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "term.h"	/* exit_ca_mode, non_rev_rmcup */

static time_t	dumptime;

WINDOW *getwin(FILE *filep)
{
	WINDOW	try, *nwin;
	int	n;

	(void) fread(&try, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		return (WINDOW *)NULL;

	if ((nwin = newwin(try._maxy+1, try._maxx+1, 0, 0)) == (WINDOW *)NULL)
		return (WINDOW *)NULL;

	/*
	 * We deliberately do not restore the _parx, _pary, or _parent
	 * fields, because the window hierarchy within which they
	 * made sense is probably gone.
	 */
	nwin->_curx       = try._curx;
	nwin->_cury       = try._cury;
	nwin->_maxy       = try._maxy;
	nwin->_maxx       = try._maxx;       
	nwin->_begy       = try._begy;
	nwin->_begx       = try._begx;
	nwin->_flags      = try._flags;

	nwin->_attrs      = try._attrs;
	nwin->_bkgd	  = try._bkgd; 

	nwin->_clear      = try._clear;
	nwin->_scroll     = try._scroll;
	nwin->_leaveok    = try._leaveok;
	nwin->_use_keypad = try._use_keypad;
	nwin->_delay   	  = try._delay;
	nwin->_immed	  = try._immed;
	nwin->_sync	  = try._sync;

	nwin->_regtop     = try._regtop;
	nwin->_regbottom  = try._regbottom;

	for (n = 0; n < nwin->_maxy + 1; n++)
	{
		(void) fread(nwin->_line[n].text,
			      sizeof(chtype), (size_t)(nwin->_maxx + 1), filep);
		if (ferror(filep))
		{
			delwin(nwin);
			return((WINDOW *)NULL);
		}
	}
	touchwin(nwin);

	return nwin;
}

int putwin(WINDOW *win, FILE *filep)
{
	int	n;

	(void) fwrite(win, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		return ERR;

	for (n = 0; n < win->_maxy + 1; n++)
	{
		(void) fwrite(win->_line[n].text,
			      sizeof(chtype), (size_t)(win->_maxx + 1), filep);
		if (ferror(filep))
			return(ERR);
	}

	return(OK);
}

int scr_restore(const char *file)
{
	FILE	*fp;

	if ((fp = fopen(file, "r")) == (FILE *)NULL)
	    return ERR;
	else
	{
	    delwin(newscr);
	    newscr = getwin(fp);
	    (void) fclose(fp);
	    return OK;
	}
}

int scr_dump(const char *file)
{
	FILE	*fp;

	if ((fp = fopen(file, "w")) == (FILE *)NULL)
	    return ERR;
	else
	{
	    (void) putwin(newscr, fp);
	    (void) fclose(fp);
	    dumptime = time((time_t *)0);
	    return OK;
	}
}

int scr_init(const char *file)
{
	FILE	*fp;
	struct stat	stb;

#ifdef exit_ca_mode
	if (exit_ca_mode && non_rev_rmcup)
	    return(ERR);
#endif /* exit_ca_mode */

	if ((fp = fopen(file, "r")) == (FILE *)NULL)
	    return ERR;
	else if (fstat(STDOUT_FILENO, &stb) || stb.st_mtime > dumptime)
	    return ERR;
	else
	{
	    delwin(curscr);
	    curscr = getwin(fp);
	    (void) fclose(fp);
	    return OK;
	}
}

int scr_set(const char *file)
{
    if (scr_init(file) == ERR)
	return(ERR);
    else
    {
	delwin(newscr);
	newscr = dupwin(curscr);
	return(OK);
    }
}


