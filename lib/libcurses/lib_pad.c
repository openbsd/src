
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
 * lib_pad.c
 * newpad	-- create a new pad
 * pnoutrefresh -- refresh a pad, no update
 * pechochar	-- add a char to a pad and refresh
 */

#include "curses.priv.h"

#include <stdlib.h>
#include <errno.h>

#if !HAVE_EXTERN_ERRNO
extern int errno;
#endif

WINDOW *newpad(int l, int c)
{
WINDOW *win;
chtype *ptr;
int i, j;

	T(("newpad(%d, %d) called", l, c));

	if (l <= 0 || c <= 0)
		return NULL;

	if ((win = _nc_makenew(l,c,0,0)) == NULL)
		return NULL;

	win->_flags |= _ISPAD;

	for (i = 0; i < l; i++) {
	    win->_line[i].oldindex = _NEWINDEX;
	    if ((win->_line[i].text = (chtype *) calloc((size_t)c, sizeof(chtype))) == NULL) {
			for (j = 0; j < i; j++)
			    free(win->_line[j].text);

			free(win->_line);
			free(win);

			errno = ENOMEM;
			return NULL;
	    }
	    else
		for (ptr = win->_line[i].text; ptr < win->_line[i].text + c; )
		    *ptr++ = ' ';
	}

	T(("newpad: returned window is %p", win));

	return(win);
}

WINDOW *subpad(WINDOW *orig, int l, int c, int begy, int begx)
{
WINDOW	*win;

	T(("subpad(%d, %d) called", l, c));

	if ((win = derwin(orig, l, c, begy, begx)) == NULL)
	    return NULL;

	win->_flags |= _ISPAD;

	T(("subpad: returned window is %p", win));

	return(win);
}

int prefresh(WINDOW *win, int pminrow, int pmincol,
	int sminrow, int smincol, int smaxrow, int smaxcol)
{
	T(("prefresh() called"));
	if (pnoutrefresh(win, pminrow, pmincol, sminrow, smincol, smaxrow, smaxcol) != ERR
	 && doupdate() != ERR) {
		return OK;
	}
	return ERR;
}

int pnoutrefresh(WINDOW *win, int pminrow, int pmincol,
	int sminrow, int smincol, int smaxrow, int smaxcol)
{
short	i, j;
short	m, n;
short	pmaxrow;
short	pmaxcol;
short	displaced;
bool	wide;

	T(("pnoutrefresh(%p, %d, %d, %d, %d, %d, %d) called", 
		win, pminrow, pmincol, sminrow, smincol, smaxrow, smaxcol));

	if (win == 0)
		return ERR;

	if (!(win->_flags & _ISPAD))
		return ERR;

	/* negative values are interpreted as zero */
	if (pminrow < 0) pminrow = 0;
	if (pmincol < 0) pmincol = 0;
	if (sminrow < 0) sminrow = 0;
	if (smincol < 0) smincol = 0;

	if (smaxrow > screen_lines
	 || smaxcol > screen_columns
	 || sminrow > smaxrow
	 || smincol > smaxcol)
		return ERR;

	pmaxrow = pminrow + smaxrow - sminrow;
	pmaxcol = pmincol + smaxcol - smincol;

	T((" pminrow + smaxrow - sminrow %d, win->_maxy %d", pmaxrow, win->_maxy));
	T((" pmincol + smaxcol - smincol %d, win->_maxx %d", pmaxcol, win->_maxx));
	if ((pmaxrow > win->_maxy)
	 || (pmaxcol > win->_maxx))
		return ERR;

	T(("pad being refreshed"));

	if (win->_pad._pad_y >= 0) {
		displaced = pminrow - win->_pad._pad_y
			  -(sminrow - win->_pad._pad_top);
		T(("pad being shifted by %d line(s)", displaced));
	} else
		displaced = 0;

	/*
	 * For pure efficiency, we'd want to transfer scrolling information
	 * from the pad to newscr whenever the window is wide enough that
	 * its update will dominate the cost of the update for the horizontal
	 * band of newscr that it occupies.  Unfortunately, this threshold
	 * tends to be complex to estimate, and in any case scrolling the
	 * whole band and rewriting the parts outside win's image would look
	 * really ugly.  So.  What we do is consider the pad "wide" if it
	 * either (a) occupies the whole width of newscr, or (b) occupies
	 * all but at most one column on either vertical edge of the screen
	 * (this caters to fussy people who put boxes around full-screen
	 * windows).  Note that changing this formula will not break any code,
	 * merely change the costs of various update cases.
	 */
	wide = (sminrow <= 1 && win->_maxx >= (newscr->_maxx - 1));

	for (i = pminrow, m = sminrow; i <= pmaxrow; i++, m++) {
		register struct ldat	*nline = &newscr->_line[m];
		register struct ldat	*oline = &win->_line[i];

		for (j = pmincol, n = smincol; j <= pmaxcol; j++, n++) {
	    		if (oline->text[j] != nline->text[n]) {
				nline->text[n] = oline->text[j];

				if (nline->firstchar == _NOCHANGE)
		   			nline->firstchar = nline->lastchar = n;
				else if (n < nline->firstchar)
		   			nline->firstchar = n;
				else if (n > nline->lastchar)
		   			nline->lastchar = n;
			}
		}

  		if (wide) {
 		    int nind = m + displaced;
 		    if (oline->oldindex < 0
 		     || nind < sminrow
 		     || nind > smaxrow)
 		    	nind = _NEWINDEX;
  
 		    nline->oldindex = nind;
  		}
		oline->firstchar = oline->lastchar = _NOCHANGE;
		oline->oldindex = i;
	}

 	/*
 	 * Clean up debris from scrolling or resizing the pad, so we do not
 	 * accidentally pick up the index value during the next call to this
 	 * procedure.  The only rows that should have an index value are those
 	 * that are displayed during this cycle.
 	 */
 	for (i = pminrow-1; (i >= 0) && (win->_line[i].oldindex >= 0); i--)
 		win->_line[i].oldindex = _NEWINDEX;
 	for (i = pmaxrow+1; (i <= win->_maxy) && (win->_line[i].oldindex >= 0); i++)
 		win->_line[i].oldindex = _NEWINDEX;
 
	win->_begx = smincol;
	win->_begy = sminrow;

	if (win->_clear) {
	    win->_clear = FALSE;
	    newscr->_clear = TRUE;
	}

	/*
	 * Use the pad's current position, if it will be visible.
	 * If not, don't do anything; it's not an error.
	 */
	if (win->_leaveok == FALSE
	 && win->_cury  >= pminrow
	 && win->_curx  >= pmincol
	 && win->_cury  <= pmaxrow
	 && win->_curx  <= pmaxcol) {
		newscr->_cury = win->_cury - pminrow + win->_begy;
		newscr->_curx = win->_curx - pmincol + win->_begx;
	}
	win->_flags &= ~_HASMOVED;

	/*
	 * Update our cache of the line-numbers that we displayed from the pad. 
	 * We will use this on subsequent calls to this function to derive
	 * values to stuff into 'oldindex[]' -- for scrolling optimization.
	 */
	win->_pad._pad_y      = pminrow;
	win->_pad._pad_x      = pmincol;
	win->_pad._pad_top    = sminrow;
	win->_pad._pad_left   = smincol;
	win->_pad._pad_bottom = smaxrow;
	win->_pad._pad_right  = smaxcol;

	return OK;
}

int pechochar(WINDOW *pad, chtype ch)
{
	T(("echochar(%p, %lx)", pad, ch));

	if (pad->_flags & _ISPAD)
		return ERR;

	waddch(curscr, ch);
	doupdate();
	return OK;
}

