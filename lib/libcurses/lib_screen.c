
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

#include <sys/stat.h>
#include <time.h>
#include <term.h>	/* exit_ca_mode, non_rev_rmcup */

MODULE_ID("Id: lib_screen.c,v 1.7 1997/02/02 00:41:10 tom Exp $")

static time_t	dumptime;

WINDOW *getwin(FILE *filep)
{
	WINDOW	try, *nwin;
	int	n;

	T((T_CALLED("getwin(%p)"), filep));

	(void) fread(&try, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		returnWin(0);

	if ((nwin = newwin(try._maxy+1, try._maxx+1, 0, 0)) == 0)
		returnWin(0);

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
	nwin->_yoffset    = try._yoffset;
	nwin->_flags      = try._flags & ~(_SUBWIN|_ISPAD);

	nwin->_attrs      = try._attrs;
	nwin->_bkgd       = try._bkgd;

	nwin->_clear      = try._clear;
	nwin->_scroll     = try._scroll;
	nwin->_leaveok    = try._leaveok;
	nwin->_use_keypad = try._use_keypad;
	nwin->_delay      = try._delay;
	nwin->_immed      = try._immed;
	nwin->_sync       = try._sync;

	nwin->_regtop     = try._regtop;
	nwin->_regbottom  = try._regbottom;

	for (n = 0; n < nwin->_maxy + 1; n++)
	{
		(void) fread(nwin->_line[n].text,
			      sizeof(chtype), (size_t)(nwin->_maxx + 1), filep);
		if (ferror(filep))
		{
			delwin(nwin);
			returnWin(0);
		}
	}
	touchwin(nwin);

	returnWin(nwin);
}

int putwin(WINDOW *win, FILE *filep)
{
	int	n;

	T((T_CALLED("putwin(%p,%p)"), win, filep));

	(void) fwrite(win, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		returnCode(ERR);

	for (n = 0; n < win->_maxy + 1; n++)
	{
		(void) fwrite(win->_line[n].text,
			      sizeof(chtype), (size_t)(win->_maxx + 1), filep);
		if (ferror(filep))
			returnCode(ERR);
	}

	returnCode(OK);
}

int scr_restore(const char *file)
{
	FILE	*fp;

	T((T_CALLED("scr_restore(%s)"), _nc_visbuf(file)));

	if ((fp = fopen(file, "r")) == 0)
	    returnCode(ERR);
	else
	{
	    delwin(newscr);
	    newscr = getwin(fp);
	    (void) fclose(fp);
	    returnCode(OK);
	}
}

int scr_dump(const char *file)
{
	FILE	*fp;

	T((T_CALLED("scr_dump(%s)"), _nc_visbuf(file)));

	if ((fp = fopen(file, "w")) == 0)
	    returnCode(ERR);
	else
	{
	    (void) putwin(newscr, fp);
	    (void) fclose(fp);
	    dumptime = time((time_t *)0);
	    returnCode(OK);
	}
}

int scr_init(const char *file)
{
	FILE	*fp;
	struct stat	stb;

	T((T_CALLED("scr_init(%s)"), _nc_visbuf(file)));

#ifdef exit_ca_mode
	if (exit_ca_mode && non_rev_rmcup)
	    returnCode(ERR);
#endif /* exit_ca_mode */

	if ((fp = fopen(file, "r")) == 0)
	    returnCode(ERR);
	else if (fstat(STDOUT_FILENO, &stb) || stb.st_mtime > dumptime)
	    returnCode(ERR);
	else
	{
	    delwin(curscr);
	    curscr = getwin(fp);
	    (void) fclose(fp);
	    returnCode(OK);
	}
}

int scr_set(const char *file)
{
    T((T_CALLED("scr_set(%s)"), _nc_visbuf(file)));

    if (scr_init(file) == ERR)
	returnCode(ERR);
    else
    {
	delwin(newscr);
	newscr = dupwin(curscr);
	returnCode(OK);
    }
}
