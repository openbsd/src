/*	$OpenBSD: lib_screen.c,v 1.4 1998/01/17 16:27:36 millert Exp $	*/


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

MODULE_ID("Id: lib_screen.c,v 1.10 1997/12/27 19:48:58 tom Exp $")

static time_t	dumptime;

WINDOW *getwin(FILE *filep)
{
	WINDOW	tmp, *nwin;
	int	n;

	T((T_CALLED("getwin(%p)"), filep));

	(void) fread(&tmp, sizeof(WINDOW), 1, filep);
	if (ferror(filep))
		returnWin(0);

	if ((nwin = newwin(tmp._maxy+1, tmp._maxx+1, 0, 0)) == 0)
		returnWin(0);

	/*
	 * We deliberately do not restore the _parx, _pary, or _parent
	 * fields, because the window hierarchy within which they
	 * made sense is probably gone.
	 */
	nwin->_curx       = tmp._curx;
	nwin->_cury       = tmp._cury;
	nwin->_maxy       = tmp._maxy;
	nwin->_maxx       = tmp._maxx;
	nwin->_begy       = tmp._begy;
	nwin->_begx       = tmp._begx;
	nwin->_yoffset    = tmp._yoffset;
	nwin->_flags      = tmp._flags & ~(_SUBWIN|_ISPAD);

	nwin->_attrs      = tmp._attrs;
	nwin->_bkgd       = tmp._bkgd;

	nwin->_clear      = tmp._clear;
	nwin->_scroll     = tmp._scroll;
	nwin->_leaveok    = tmp._leaveok;
	nwin->_use_keypad = tmp._use_keypad;
	nwin->_delay      = tmp._delay;
	nwin->_immed      = tmp._immed;
	nwin->_sync       = tmp._sync;

	nwin->_regtop     = tmp._regtop;
	nwin->_regbottom  = tmp._regbottom;

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
        int code = ERR; 
	int n;

	T((T_CALLED("putwin(%p,%p)"), win, filep));

	if (win) {
	  (void) fwrite(win, sizeof(WINDOW), 1, filep);
	  if (ferror(filep))
	    returnCode(code);

	  for (n = 0; n < win->_maxy + 1; n++)
	    {
	      (void) fwrite(win->_line[n].text,
			    sizeof(chtype), (size_t)(win->_maxx + 1), filep);
	      if (ferror(filep))
		returnCode(code);
	    }
	  code = OK;
	}
	returnCode(code);
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
