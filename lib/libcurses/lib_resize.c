
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
 * Note: This code is not part of the SVr4/XSI Curses API!
 */

#include "curses.priv.h"
#include <stdlib.h>

int wresize(WINDOW *win, int new_lines, int new_cols)
{
  chtype	blank	= _nc_render(win, ' ', BLANK);
  register int	i, j;

  T(("wresize(win=%p, lines=%d, cols=%d) called", win, new_lines, new_cols));

  if (new_lines <= 0 || new_cols <= 0)
    return ERR;

  /* window height is different, must mess with the line vector */
  if (new_lines != win->_maxy+1)
  {
    struct ldat *tp;

    /* free old lines no longer used */
    if (!(win->_flags & _SUBWIN))
      for (i = new_lines + 1; i <= win->_maxy; i++)
	free((char *)(win->_line[i].text));

    /* resize the window line vector */
    if (!(win->_line = realloc(win->_line, sizeof(struct ldat) * new_lines)))
      return(ERR);

    /* grab new lines for the window if needed */
    for (tp=&win->_line[i=win->_maxy+1]; tp<&win->_line[new_lines]; i++,tp++)
    {
      if (win->_flags & _SUBWIN)	/* set up alias pointers */
        tp->text = &(win->_parent->_line[win->_pary+i].text[win->_parx]);
      else				/* allocate new lines if needed */
      {
	if (!(tp->text = (chtype *)malloc(sizeof(chtype) * new_cols)))
	  return(ERR);
	for (j = 0; j < new_cols; j++)
	  tp->text[j] = blank;
      }

      tp->firstchar = 0;
      tp->lastchar  = new_cols;
      tp->oldindex  = i;
    }

    /*
     * This is kind of nasty.  We have to clip the scrolling region to
     * within the new window size.  We also have to assume that if the
     * bottom of the scrolling region is the last line, the user wants
     * that bottom to stick to the bottom of the resized window.  The
     * real problem here is that the API doesn't distinguish between 
     * resetting the scroll region to the entire window and setting it
     * to an explicit scroll region that happens to include the whole
     * window.
     */
    if (win->_regtop > new_lines - 1 || win->_regtop == win->_maxy)
      win->_regtop = new_lines - 1;
    if (win->_regbottom > new_lines - 1 || win->_regbottom == win->_maxy)
      win->_regbottom = new_lines - 1;
  }

  /* window width is different, resize all old lines */ 
  if (new_cols != win->_maxx+1)
    for (i = 0; i < min(new_lines, win->_maxy+1); i++)
    {
      /* if not a subwindow, we have our own storage; resize each line */
      if (!(win->_flags & _SUBWIN))
      {
	win->_line[i].text=realloc(win->_line[i].text,sizeof(chtype)*new_cols);
	if (win->_line[i].text == (chtype *)NULL)
	  return(ERR);
      }

      if (new_cols > win->_maxx+1)	/* window is growing horizontally */
      {
	if (win->_line[i].firstchar == _NOCHANGE)
	  win->_line[i].firstchar = win->_maxx+1;
	win->_line[i].lastchar = new_cols;
	for (j = win->_maxx+1; j < new_cols; j++)	/* blank-fill ends */
	  win->_line[i].text[j] = blank;
      }
      else	/* window is shrinking horizontally */
      {
	if (win->_line[i].firstchar > win->_maxx+1)
	  win->_line[i].firstchar = _NOCHANGE;
	else if (win->_line[i].lastchar > new_cols)
	  win->_line[i].lastchar = new_cols;
      }
    }

  /* clip the cursor position to within the new size */
  if (win->_curx > new_cols - 1) 
    win->_curx = new_cols - 1;
  if (win->_cury > new_lines - 1)
    win->_cury = new_lines - 1;

  /* whether this is a full-width or full-depth window may have changed */
  win->_flags &=~ (_ENDLINE|_FULLWIN|_SCROLLWIN);
  if (win->_begx + new_cols == screen_columns)
  {
    win->_flags |= _ENDLINE;

    if (win->_begx == 0 && new_lines == screen_lines && win->_begy == 0)
      win->_flags |= _FULLWIN;

    if (win->_begy + new_lines == screen_lines)
      win->_flags |= _SCROLLWIN;
  }

  /* right margin may have moved, set _NEED_WRAP properly */
  if ((win->_flags & _NEED_WRAP) && win->_curx != new_cols - 1)
  {
    win->_curx++;
    win->_flags &=~ _NEED_WRAP;
  }
  if (!(win->_flags & _NEED_WRAP) && win->_curx == new_cols)
  {
    win->_curx--;
    win->_flags |= _NEED_WRAP;
  }

  /* finally, update size members */
  win->_maxy = new_lines - 1;
  win->_maxx = new_cols - 1;

  return OK;
}
