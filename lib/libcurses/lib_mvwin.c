/*	$OpenBSD: lib_mvwin.c,v 1.4 1997/12/14 23:15:47 millert Exp $	*/


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
**	lib_mvwin.c
**
**	The routine mvwin().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_mvwin.c,v 1.6 1997/12/03 15:47:41 Alexander.V.Lukyanov Exp $")

int mvwin(WINDOW *win, int by, int bx)
{
	T((T_CALLED("mvwin(%p,%d,%d)"), win, by, bx));

	if (!win || (win->_flags & _ISPAD))
	    returnCode(ERR);

	/* Copying subwindows is allowed, but it is expensive... */
	if (win->_flags & _SUBWIN) {
	  int err = ERR;
	  WINDOW *parent = win->_parent;
	  if (parent)
	    { /* Now comes the complicated and costly part, you should really
	       * try to avoid to move subwindows. Because a subwindow shares
	       * the text buffers with its parent, one can't do a simple
	       * memmove of the text buffers. One has to create a copy, then
	       * to relocate the subwindow and then to do a copy.
	       */
	      if ((by - parent->_begy == win->_pary) &&
		  (bx - parent->_begx == win->_parx))
		err=OK; /* we don't actually move */
	      else {
		WINDOW* clone = dupwin(win);  
		if (clone) {
		  /* now we have the clone, so relocate win */
		  
		  werase(win);             /* Erase the original place     */
		  wbkgd(win,parent->_bkgd);/* fill with parents background */
		  wsyncup(win);            /* Tell the parent(s)           */
		  
		  err = mvderwin(win,                   
				 by - parent->_begy,
				 bx - parent->_begx);
		  if (err!=ERR) {
		    err = copywin(clone,win,
				  0, 0, 0, 0, win->_maxy, win->_maxx, 0);
		    if (ERR!=err)
		      wsyncup(win);
		  }
		  if (ERR==delwin(clone))
		    err=ERR;
		}
	      }
	    }
	  returnCode(err);
	}

	if (by + win->_maxy > screen_lines - 1
	||  bx + win->_maxx > screen_columns - 1
	||  by < 0
	||  bx < 0)
	    returnCode(ERR);

	/*
	 * Whether or not the window is moved, touch the window's contents so
	 * that a following call to 'wrefresh()' will paint the window at the
	 * new location.  This ensures that if the caller has refreshed another
	 * window at the same location, that this one will be displayed.
	 */	
	win->_begy = by;
	win->_begx = bx;
	returnCode(touchwin(win));
}
