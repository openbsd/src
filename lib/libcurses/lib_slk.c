
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
 *	lib_slk.c
 *	Soft key routines.
 */

#include <curses.priv.h>

#include <ctype.h>
#include <term.h>	/* num_labels, label_*, plab_norm */

MODULE_ID("Id: lib_slk.c,v 1.11 1997/01/18 23:06:32 tom Exp $")

#define MAX_SKEY_OLD	   8	/* count of soft keys */
#define MAX_SKEY_LEN_OLD   8	/* max length of soft key text */
#define MAX_SKEY_PC       12    /* This is what most PC's have */
#define MAX_SKEY_LEN_PC    5

#define MAX_SKEY      (SLK_STDFMT ? MAX_SKEY_OLD : MAX_SKEY_PC)
#define MAX_SKEY_LEN  (SLK_STDFMT ? MAX_SKEY_LEN_OLD : MAX_SKEY_LEN_PC)
/*
 * We'd like to move these into the screen context structure, but cannot,
 * because slk_init() is called before initscr()/newterm().
 */
int _nc_slk_format;			/* one more than format specified in slk_init() */

static chtype _slk_attr = A_STANDOUT;	/* soft label attribute */
static SLK *_slk;
static void slk_paint_info(WINDOW *win);

/*
 * Fetch the label text.
 */

char *
slk_label(int n)
{
	T(("slk_label(%d)", n));

	if (SP->_slk == NULL || n < 1 || n > SP->_slk->labcnt)
		return NULL;
	return(SP->_slk->ent[n-1].text);
}

/*
 * Write the soft labels to the soft-key window.
 */

static void
slk_intern_refresh(SLK *slk)
{
int i;
	for (i = 0; i < slk->labcnt; i++) {
		if (slk->dirty || slk->ent[i].dirty) {
			if (slk->ent[i].visible) {
#ifdef num_labels
				if (num_labels > 0 && SLK_STDFMT)
				{
				  if (i < num_labels) {
				    TPUTS_TRACE("plab_norm");
				    putp(tparm(plab_norm, i, slk->win,slk->ent[i].form_text));
				  }
				}
				else
#endif /* num_labels */
				{
					wmove(slk->win,SLK_LINES-1,slk->ent[i].x);
					wattrset(slk->win,_slk_attr);
					waddnstr(slk->win,slk->ent[i].form_text, MAX_SKEY_LEN);
					/* if we simulate SLK's, it's looking much more
					   natural to use the current ATTRIBUTE also
					   for the label window */
					wattrset(slk->win,stdscr->_attrs);
				}
			}
			slk->ent[i].dirty = FALSE;
		}
	}
	slk->dirty = FALSE;

#ifdef num_labels
	if (num_labels > 0)
	    if (slk->hidden)
	    {
		TPUTS_TRACE("label_off");
		putp(label_off);
	    }
	    else
	    {
		TPUTS_TRACE("label_on");
		putp(label_on);
	    }
#endif /* num_labels */
}

/*
 * Refresh the soft labels.
 */

int
slk_noutrefresh(void)
{
	T(("slk_noutrefresh()"));

	if (SP->_slk == NULL)
		return(ERR);
	if (SP->_slk->hidden)
		return(OK);
	slk_intern_refresh(SP->_slk);
	return(wnoutrefresh(SP->_slk->win));
}

/*
 * Refresh the soft labels.
 */

int
slk_refresh(void)
{
	T(("slk_refresh()"));

	if (SP->_slk == NULL)
		return(ERR);
	if (SP->_slk->hidden)
		return(OK);
	slk_intern_refresh(SP->_slk);
	return(wrefresh(SP->_slk->win));
}

/*
 * Restore the soft labels on the screen.
 */

int
slk_restore(void)
{
	T(("slk_restore()"));

	if (SP->_slk == NULL)
		return(ERR);
	SP->_slk->hidden = FALSE;
	SP->_slk->dirty = TRUE;
	/* we have to repaint info line eventually */
	slk_paint_info(SP->_slk->win); 
	return slk_refresh();
}

/*
 * Set soft label text.
 */

int
slk_set(int i, const char *astr, int format)
{
SLK *slk = SP->_slk;
size_t len;
const char *str = astr;
const char *p;

	T(("slk_set(%d, \"%s\", %d)", i, str, format));

	if (slk == NULL || i < 1 || i > slk->labcnt || format < 0 || format > 2)
		return(ERR);
	if (str == NULL)
		str = "";

	while (isspace(*str)) str++; /* skip over leading spaces  */
	p = str;
	while (isprint(*p)) p++;     /* The first non-print stops */

	--i; /* Adjust numbering of labels */

	len = (size_t)(p - str);
	if (len > (unsigned)slk->maxlen)
	  len = slk->maxlen;
	if (len==0)
	  slk->ent[i].text[0] = 0;
	else
	  (void) strncpy(slk->ent[i].text, str, len);
	memset(slk->ent[i].form_text,' ', (unsigned)slk->maxlen);
	slk->ent[i].text[slk->maxlen] = 0;
	/* len = strlen(slk->ent[i].text); */

	switch(format) {
	case 0: /* left-justified */
		memcpy(slk->ent[i].form_text,
		       slk->ent[i].text,
		       len);
		break;
	case 1: /* centered */
		memcpy(slk->ent[i].form_text+(slk->maxlen - len)/2,
		       slk->ent[i].text,
		       len);
		break;
	case 2: /* right-justified */
		memcpy(slk->ent[i].form_text+ slk->maxlen - len,
		       slk->ent[i].text,
		       len);
		break;
	}
	slk->ent[i].form_text[slk->maxlen] = 0;
	slk->ent[i].dirty = TRUE;
	return(OK);
}

/*
 * Force the code to believe that the soft keys have been changed.
 */

int
slk_touch(void)
{
	T(("slk_touch()"));

	if (SP->_slk == NULL)
		return(ERR);
	SP->_slk->dirty = TRUE;
	return(OK);
}

/*
 * Remove soft labels from the screen.
 */

int
slk_clear(void)
{
	T(("slk_clear()"));

	if (SP->_slk == NULL)
		return(ERR);
	SP->_slk->hidden = TRUE;
	/* For simulated SLK's it's looks much more natural to
	   inherit those attributes from the standard screen */
	SP->_slk->win->_bkgd  = stdscr->_bkgd;
	SP->_slk->win->_attrs = stdscr->_attrs;
	werase(SP->_slk->win);
	return wrefresh(SP->_slk->win);
}

/*
 * Paint the info line for the PC style SLK emulation.
 * 
 */

static void
slk_paint_info(WINDOW *win)
{
  if (win && _nc_slk_format==4)
    {
      int i;
  
      mvwhline (win,0,0,0,getmaxx(win));
      wmove (win,0,0);
      
      for (i = 0; i < _slk->maxlab; i++) {
	if (win && _nc_slk_format==4)
	  {
	    mvwaddch(win,0,_slk->ent[i].x,'F');
	    if (i<9)
	      waddch(win,'1'+i);
	    else
	      {
		waddch(win,'1');
		waddch(win,'0' + (i-9));
	      }
	  }
      }
    }
}

/*
 * Initialize soft labels.
 * Called from newterm()
 */

int
_nc_slk_initialize(WINDOW *stwin, int cols)
{
int i, x;
char *p;

	T(("slk_initialize()"));

	if (_slk)
	  { /* we did this already, so simply return */
	    SP->_slk = _slk;
	    return(OK);
	  }
	else
	  if ((SP->_slk = _slk = typeCalloc(SLK, 1)) == 0)
	    return(ERR);

	_slk->ent = NULL;
	_slk->buffer = NULL;

#ifdef num_labels
	_slk->maxlab = (num_labels > 0) ? num_labels : MAX_SKEY;
	_slk->maxlen = (num_labels > 0) ? label_width * label_height : MAX_SKEY_LEN;
	_slk->labcnt = (_slk->maxlab < MAX_SKEY) ? MAX_SKEY : _slk->maxlab;
#else
	_slk->labcnt = _slk->maxlab = MAX_SKEY;
	_slk->maxlen = MAX_SKEY_LEN;
#endif /* num_labels */

	_slk->ent = typeCalloc(slk_ent, _slk->labcnt);
	if (_slk->ent == NULL)
	  goto exception;

	p = _slk->buffer = (char*) calloc(2*_slk->labcnt,(1+_slk->maxlen));
	if (_slk->buffer == NULL)
	  goto exception;

	for (i = 0; i < _slk->labcnt; i++) {
		_slk->ent[i].text = p;
		p += (1 + _slk->maxlen);
		_slk->ent[i].form_text = p;
		p += (1 + _slk->maxlen);
		memset(_slk->ent[i].form_text, ' ', (unsigned)_slk->maxlen);
		_slk->ent[i].visible = (i < _slk->maxlab);
	}
	if (_nc_slk_format >= 3) /* PC style */
	  {
	    int gap = (cols - 3 * (3 + 4*_slk->maxlen))/2;

	    if (gap < 1)
	      gap = 1;

	    for (i = x = 0; i < _slk->maxlab; i++) {
	      _slk->ent[i].x = x;
	      x += _slk->maxlen;
	      x += (i==3 || i==7) ? gap : 1;
	    }
	    if (_nc_slk_format == 4)
	      slk_paint_info (stwin);
	  }
	else {
	  if (_nc_slk_format == 2) {	/* 4-4 */
	    int gap = cols - (_slk->maxlab * _slk->maxlen) - 6;

	    if (gap < 1)
			gap = 1;
	    for (i = x = 0; i < _slk->maxlab; i++) {
	      _slk->ent[i].x = x;
	      x += _slk->maxlen;
	      x += (i == 3) ? gap : 1;
	    }
	  }
	  else
	    {
	      if (_nc_slk_format == 1) { /* 1 -> 3-2-3 */
		int gap = (cols - (_slk->maxlab * _slk->maxlen) - 5) / 2;

		if (gap < 1)
		  gap = 1;
		for (i = x = 0; i < _slk->maxlab; i++) {
		  _slk->ent[i].x = x;
		  x += _slk->maxlen;
		  x += (i == 2 || i == 4) ? gap : 1;
		}
	      }
	      else
		goto exception;
	    }
	}
	_slk->dirty = TRUE;
	if ((_slk->win = stwin) == NULL)
	{
	exception:
		if (_slk)
		{
		   FreeIfNeeded(_slk->buffer);
		   FreeIfNeeded(_slk->ent);
		   free(_slk);
		   SP->_slk = _slk = (SLK*)0;
		   return(ERR);
		}
	}

	return(OK);
}

/*
 * Initialize soft labels.  Called by the user before initscr().
 */

int
slk_init(int format)
{
	if (format < 0 || format > 3)
		return(ERR);
	_nc_slk_format = 1 + format;
	return(OK);
}

/* Functions to manipulate the soft-label attribute */

int
slk_attrset(const attr_t attr)
{
    _slk_attr = attr;
    return(OK);
}

int
slk_attron(const attr_t attr)
{
    toggle_attr_on(_slk_attr,attr);
    return(OK);
}

int
slk_attroff(const attr_t attr)
{
    toggle_attr_off(_slk_attr,attr);
    return(OK);
}

attr_t
slk_attr(void)
{
  return _slk_attr;
}
