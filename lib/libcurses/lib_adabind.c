/*	$OpenBSD: lib_adabind.c,v 1.1 1998/07/23 21:18:25 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <Juergen.Pfeifer@T-Online.de> 1996,1997        *
 ****************************************************************************/

/*
//	lib_adabind.c
//
//	Some small wrappers to ease the implementation of an Ada95
//      binding. Especially functionalities only available as macros
//      in (n)curses are wrapped here by functions. 
//      See the documentation and copyright notices in the ../Ada95
//      subdirectory.
*/
#include "curses.priv.h"

MODULE_ID("$From: lib_adabind.c,v 1.7 1998/02/11 12:13:59 tom Exp $")

/*  In (n)curses are a few functionalities that can't be expressed as 
//  functions, because for historic reasons they use as macro argument
//  variable names that are "out" parameters. For those macros we provide
//  small wrappers.
*/

/* Prototypes for the functions in this module */
int  _nc_ada_getmaxyx (const WINDOW *win, int *y, int *x);
int  _nc_ada_getbegyx (const WINDOW *win, int *y, int *x);
int  _nc_ada_getyx    (const WINDOW *win, int *y, int *x);
int  _nc_ada_getparyx (const WINDOW *win, int *y, int *x);
int  _nc_ada_isscroll (const WINDOW *win);
int  _nc_ada_coord_transform (const WINDOW *win, int *Y, int *X, int dir);
void _nc_ada_mouse_event (mmask_t m, int *b, int *s);
int  _nc_ada_mouse_mask (int button, int state, mmask_t *mask);
void _nc_ada_unregister_mouse (void);
int  _nc_ada_vcheck (int major, int  minor);

int _nc_ada_getmaxyx (const WINDOW *win, int *y, int *x)
{
  if (win && y && x)
    {
      getmaxyx(win,*y,*x);
      return OK;
    }
  else
    return ERR;
}

int _nc_ada_getbegyx (const WINDOW *win, int *y, int *x)
{
  if (win && y && x)
    {
      getbegyx(win,*y,*x);
      return OK;
    }
  else
    return ERR;
}

int _nc_ada_getyx (const WINDOW *win, int *y, int *x)
{
  if (win && y && x)
    {
      getyx(win,*y,*x);
      return OK;
    }
  else
    return ERR;
}

int _nc_ada_getparyx (const WINDOW *win, int *y, int *x)
{
  if (win && y && x)
    {
      getparyx(win,*y,*x);
      return OK;
    }
  else
    return ERR;
}

int _nc_ada_isscroll (const WINDOW *win)
{
  return win ? (win->_scroll ? TRUE : FALSE) : ERR;
}

int _nc_ada_coord_transform (const WINDOW *win, int *Y, int *X, int dir)
{
  if (win && Y && X)
    {
      int y = *Y; int x = *X;
      if (dir)
	{ /* to screen coordinates */
	  y += win->_yoffset;
	  y += win->_begy;
	  x += win->_begx;
	  if (!wenclose(win,y,x))
	    return FALSE;
	}
      else
	{ /* from screen coordinates */
	  if (!wenclose(win,y,x))
	    return FALSE;
	  y -= win->_yoffset;
	  y -= win->_begy;
	  x -= win->_begx;
	}
      *X = x;
      *Y = y;
      return TRUE;
    }
  return FALSE;
}

#define BUTTON1_EVENTS (BUTTON1_RELEASED        |\
                        BUTTON1_PRESSED         |\
                        BUTTON1_CLICKED         |\
                        BUTTON1_DOUBLE_CLICKED  |\
                        BUTTON1_TRIPLE_CLICKED  |\
                        BUTTON1_RESERVED_EVENT  )

#define BUTTON2_EVENTS (BUTTON2_RELEASED        |\
                        BUTTON2_PRESSED         |\
                        BUTTON2_CLICKED         |\
                        BUTTON2_DOUBLE_CLICKED  |\
                        BUTTON2_TRIPLE_CLICKED  |\
                        BUTTON2_RESERVED_EVENT  )

#define BUTTON3_EVENTS (BUTTON3_RELEASED        |\
                        BUTTON3_PRESSED         |\
                        BUTTON3_CLICKED         |\
                        BUTTON3_DOUBLE_CLICKED  |\
                        BUTTON3_TRIPLE_CLICKED  |\
                        BUTTON3_RESERVED_EVENT  )

#define BUTTON4_EVENTS (BUTTON4_RELEASED        |\
                        BUTTON4_PRESSED         |\
                        BUTTON4_CLICKED         |\
                        BUTTON4_DOUBLE_CLICKED  |\
                        BUTTON4_TRIPLE_CLICKED  |\
                        BUTTON4_RESERVED_EVENT  )

void _nc_ada_mouse_event( mmask_t m, int *b, int *s )
{
  int k = 0;

  if ( m & BUTTON1_EVENTS)
    {
      k = 1;
    }
  else if ( m & BUTTON2_EVENTS)
    {
      k = 2;
    }
  else if ( m & BUTTON3_EVENTS)
    {
      k = 3;
    }
  else if ( m & BUTTON4_EVENTS)
    {
      k = 4;
    }

  if (k)
    {
      *b = k-1;
      if (BUTTON_RELEASE(m,k)) *s = 0;
      else if (BUTTON_PRESS(m,k)) *s = 1;
      else if (BUTTON_CLICK(m,k)) *s = 2;
      else if (BUTTON_DOUBLE_CLICK(m,k)) *s = 3;
      else if (BUTTON_TRIPLE_CLICK(m,k)) *s = 4;
      else if (BUTTON_RESERVED_EVENT(m,k)) *s = 5;
      else
	{
	  *s = -1;
	}
    }
  else
    {
      *s = 1;
      if (m & BUTTON_CTRL) *b = 4;
      else if (m & BUTTON_SHIFT) *b = 5;
      else if (m & BUTTON_ALT) *b = 6;
      else
	{
	  *b = -1;
	}
    }
}

int _nc_ada_mouse_mask ( int button, int state, mmask_t *mask )
{
  mmask_t b = (button<4) ? ((1<<button) << (6 * state)) :
    (BUTTON_CTRL << (button-4));

  if (button>=4 && state!=1)
    return ERR;

  *mask |= b;
  return OK;
}

/*
 * Allow Ada to check whether or not we are the correct library version
 * for the binding. It calls this routine with the version it requests
 * and this routine returns a 1 if it is a correct version, a 2 if the
 * major version is correct but the minor version of the library differs
 * and a 0 if the versions don't match.
 */
int  _nc_ada_vcheck (int major, int  minor)
{
  if (major==NCURSES_VERSION_MAJOR) {
    if (minor==NCURSES_VERSION_MINOR)
      return 1;
    else
      return 2;
  }
  else
    return 0;
}
