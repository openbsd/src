
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                     panels is copyright (C) 1995                         *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*	      All praise to the original author, Warren Tucker.            *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute panels   *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of panels in any    *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        panels comes AS IS with no warranty, implied or expressed.        *
*                                                                          *
***************************************************************************/

/* panel.c -- implementation of panels library */
#include "panel.priv.h"

MODULE_ID("$Id: panel.c,v 1.2 1997/11/26 03:56:05 millert Exp $")

#ifdef TRACE
extern char *_nc_visbuf(const char *);
#ifdef TRACE_TXT
#define USER_PTR(ptr) _nc_visbuf((const char *)ptr)
#else
static char *my_nc_visbuf(const void *ptr)
{
	char temp[40];
	if (ptr != 0)
		sprintf(temp, "ptr:%p", ptr);
	else
		strcpy(temp, "<null>");
	return _nc_visbuf(temp);
}
#define USER_PTR(ptr) my_nc_visbuf((const char *)ptr)
#endif
#endif

static PANEL *__bottom_panel = (PANEL *)0;
static PANEL *__top_panel    = (PANEL *)0;

static PANEL __stdscr_pseudo_panel = { (WINDOW *)0,
				       0,0,0,0,
				       (PANEL *)0, (PANEL *)0,
				       (void *)0,
				       (PANELCONS *)0 };

/* Prototypes */
static void __panel_link_bottom(PANEL *pan);

/*+-------------------------------------------------------------------------
	dPanel(text,pan)
--------------------------------------------------------------------------*/
#ifdef TRACE
static void
dPanel(const char *text, const PANEL *pan)
{
	_tracef("%s id=%s b=%s a=%s y=%d x=%d",
		text, USER_PTR(pan->user),
		(pan->below) ?  USER_PTR(pan->below->user) : "--",
		(pan->above) ?  USER_PTR(pan->above->user) : "--",
		pan->wstarty, pan->wstartx);
} /* end of dPanel */
#else
#  define dPanel(text,pan)
#endif

/*+-------------------------------------------------------------------------
	dStack(fmt,num,pan)
--------------------------------------------------------------------------*/
#ifdef TRACE
static void
dStack(const char *fmt, int num, const PANEL *pan)
{
  char s80[80];

  snprintf(s80,sizeof(s80),fmt,num,pan);
  _tracef("%s b=%s t=%s",s80,
	  (__bottom_panel) ?  USER_PTR(__bottom_panel->user) : "--",
	  (__top_panel)    ?  USER_PTR(__top_panel->user)    : "--");
  if(pan)
    _tracef("pan id=%s", USER_PTR(pan->user));
  pan = __bottom_panel;
  while(pan)
    {
      dPanel("stk",pan);
      pan = pan->above;
    }
} /* end of dStack */
#else
#  define dStack(fmt,num,pan)
#endif

/*+-------------------------------------------------------------------------
	Wnoutrefresh(pan) - debugging hook for wnoutrefresh
--------------------------------------------------------------------------*/
#ifdef TRACE
static void
Wnoutrefresh(const PANEL *pan)
{
  dPanel("wnoutrefresh",pan);
  wnoutrefresh(pan->win);
} /* end of Wnoutrefresh */
#else
#  define Wnoutrefresh(pan) wnoutrefresh((pan)->win)
#endif

/*+-------------------------------------------------------------------------
	Touchpan(pan)
--------------------------------------------------------------------------*/
#ifdef TRACE
static void
Touchpan(const PANEL *pan)
{
  dPanel("Touchpan",pan);
  touchwin(pan->win);
} /* end of Touchpan */
#else
#  define Touchpan(pan) touchwin((pan)->win)
#endif

/*+-------------------------------------------------------------------------
	Touchline(pan,start,count)
--------------------------------------------------------------------------*/
#ifdef TRACE
static void
Touchline(const PANEL *pan, int start, int count)
{
  char s80[80];
  snprintf(s80,sizeof(s80),"Touchline s=%d c=%d",start,count);
  dPanel(s80,pan);
  touchline(pan->win,start,count);
} /* end of Touchline */
#else
#  define Touchline(pan,start,count) touchline((pan)->win,start,count)
#endif

/*+-------------------------------------------------------------------------
	__panels_overlapped(pan1,pan2) - check panel overlapped
--------------------------------------------------------------------------*/
static INLINE bool
__panels_overlapped(register const PANEL *pan1, register const PANEL *pan2)
{
  if(!pan1 || !pan2)
    return(FALSE);
  dBug(("__panels_overlapped %s %s", USER_PTR(pan1->user), USER_PTR(pan2->user)));
  /* pan1 intersects with pan2 ? */
  if((pan1->wstarty >= pan2->wstarty) && (pan1->wstarty < pan2->wendy) &&
     (pan1->wstartx >= pan2->wstartx) && (pan1->wstartx < pan2->wendx))
    return(TRUE);
  /* or vice versa test */
  if((pan2->wstarty >= pan1->wstarty) && (pan2->wstarty < pan1->wendy) &&
     (pan2->wstartx >= pan1->wstartx) && (pan2->wstartx < pan1->wendx))
    return(TRUE);
  dBug(("  no"));
  return(FALSE);
} /* end of __panels_overlapped */

/*+-------------------------------------------------------------------------
	__free_obscure(pan)
--------------------------------------------------------------------------*/
static INLINE void
__free_obscure(PANEL *pan)
{
  PANELCONS *tobs = pan->obscure;			/* "this" one */
  PANELCONS *nobs;					/* "next" one */

  while(tobs)
    {
      nobs = tobs->above;
      free((char *)tobs);
      tobs = nobs;
    }
  pan->obscure = (PANELCONS *)0;
} /* end of __free_obscure */

/*+-------------------------------------------------------------------------
  Get root (i.e. stdscr's) panel.
  Establish the pseudo panel for stdscr if necessary.
--------------------------------------------------------------------------*/
static PANEL*
__root_panel(void)
{
  if(!__stdscr_pseudo_panel.win)
    { /* initialize those fields not already statically initialized */
      assert(stdscr && !__bottom_panel && !__top_panel);
      __stdscr_pseudo_panel.win = stdscr;
      __stdscr_pseudo_panel.wendy = LINES;
      __stdscr_pseudo_panel.wendx = COLS;
#ifdef TRACE
      __stdscr_pseudo_panel.user = "stdscr";
#endif
      __panel_link_bottom(&__stdscr_pseudo_panel);
    }
  return &__stdscr_pseudo_panel;
}

/*+-------------------------------------------------------------------------
	__override(pan,show)
--------------------------------------------------------------------------*/
static void
__override(const PANEL *pan, int show)
{
  int y;
  PANEL *pan2;
  PANELCONS *tobs = pan->obscure;			   /* "this" one */

  dBug(("__override %s,%d", USER_PTR(pan->user),show));

  switch (show)
    {
    case P_TOUCH:
      Touchpan(pan);
      /* The following while loop will now mark all panel window lines
       * obscured by use or obscuring us as touched, so they will be
       * updated.
       */
      break;
    case P_UPDATE:
      while(tobs && (tobs->pan != pan))
	tobs = tobs->above;
      /* The next loop will now only go through the panels obscuring pan;
       * it updates all the lines in the obscuring panels in sync. with
       * the lines touched in pan itself. This is called in update_panels()
       * in a loop from the bottom_panel to the top_panel, resulting in
       * the desired update effect.
       */
      break;
    default:
      return;
    }

  while(tobs)
    {
      if((pan2 = tobs->pan) != pan) {
	dBug(("test obs pan=%s pan2=%s", USER_PTR(pan->user), USER_PTR(pan2->user)));
	for(y = pan->wstarty; y < pan->wendy; y++) {
	  if( (y >= pan2->wstarty) && (y < pan2->wendy) &&
	      ((is_linetouched(pan->win,y - pan->wstarty) == TRUE)) )
	    Touchline(pan2,y - pan2->wstarty,1);
	}
      }
      tobs = tobs->above;
    }
} /* end of __override */

/*+-------------------------------------------------------------------------
	__calculate_obscure()
--------------------------------------------------------------------------*/
static void
__calculate_obscure(void)
{
  PANEL *pan;
  PANEL *pan2;
  PANELCONS *tobs;			/* "this" one */
  PANELCONS *lobs = (PANELCONS *)0;	/* last one */

  pan = __bottom_panel;
  while(pan)
    {
      if(pan->obscure)
	__free_obscure(pan);
      dBug(("--> __calculate_obscure %s", USER_PTR(pan->user)));
      lobs = (PANELCONS *)0;		/* last one */
      pan2 = __bottom_panel;
      /* This loop builds a list of panels obsured by pan or obscuring
	 pan; pan itself is in the list; all panels before pan are
	 obscured by pan, all panels after pan are obscuring pan. */
      while(pan2)
	{
	  if(__panels_overlapped(pan,pan2))
	    {
	      if(!(tobs = (PANELCONS *)malloc(sizeof(PANELCONS))))
		return;
	      tobs->pan = pan2;
	      dPanel("obscured",pan2);
	      tobs->above = (PANELCONS *)0;
	      if(lobs)
		lobs->above = tobs;
	      else
		pan->obscure = tobs;
	      lobs  = tobs;
	    }
	  pan2 = pan2->above;
	}
      __override(pan,P_TOUCH);
      pan = pan->above;
    }
} /* end of __calculate_obscure */

/*+-------------------------------------------------------------------------
	__panel_is_linked(pan) - check to see if panel is in the stack
--------------------------------------------------------------------------*/
static INLINE bool
__panel_is_linked(const PANEL *pan)
{
  /* This works! The only case where it would fail is, when the list has
     only one element. But this could only be the pseudo panel at the bottom */
  return ( ((pan->above!=(PANEL *)0) ||
	    (pan->below!=(PANEL *)0) ||
	    (pan==__bottom_panel)) ? TRUE : FALSE );
} /* end of __panel_is_linked */

/*+-------------------------------------------------------------------------
	__panel_link_top(pan) - link panel into stack at top
--------------------------------------------------------------------------*/
static void
__panel_link_top(PANEL *pan)
{
#ifdef TRACE
  dStack("<lt%d>",1,pan);
  if(__panel_is_linked(pan))
    return;
#endif

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  if(__top_panel)
    {
      __top_panel->above = pan;
      pan->below = __top_panel;
    }
  __top_panel = pan;
  if(!__bottom_panel)
    __bottom_panel = pan;
  __calculate_obscure();
  dStack("<lt%d>",9,pan);

} /* end of __panel_link_top */

/*+-------------------------------------------------------------------------
	__panel_link_bottom(pan) - link panel into stack at bottom
--------------------------------------------------------------------------*/
static void
__panel_link_bottom(PANEL *pan)
{
#ifdef TRACE
  dStack("<lb%d>",1,pan);
  if(__panel_is_linked(pan))
    return;
#endif

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  if(__bottom_panel)
    { /* the stdscr pseudo panel always stays real bottom;
         so we insert after bottom panel*/
      pan->below = __bottom_panel;
      pan->above = __bottom_panel->above;
      if (pan->above)
	pan->above->below = pan;
      __bottom_panel->above = pan;
    }
  else
    __bottom_panel = pan;
  if(!__top_panel)
    __top_panel = pan;
  assert(__bottom_panel == &__stdscr_pseudo_panel);
  __calculate_obscure();
  dStack("<lb%d>",9,pan);
} /* end of __panel_link_bottom */

/*+-------------------------------------------------------------------------
	__panel_unlink(pan) - unlink panel from stack
--------------------------------------------------------------------------*/
static void
__panel_unlink(PANEL *pan)
{
  PANEL *prev;
  PANEL *next;

#ifdef TRACE
  dStack("<u%d>",1,pan);
  if(!__panel_is_linked(pan))
    return;
#endif

  __override(pan,P_TOUCH);
  __free_obscure(pan);

  prev = pan->below;
  next = pan->above;

  if(prev)
    { /* if non-zero, we will not update the list head */
      prev->above = next;
      if(next)
	next->below = prev;
    }
  else if(next)
    next->below = prev;
  if(pan == __bottom_panel)
    __bottom_panel = next;
  if(pan == __top_panel)
    __top_panel = prev;

  __calculate_obscure();

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  dStack("<u%d>",9,pan);
} /* end of __panel_unlink */

/*+-------------------------------------------------------------------------
	panel_window(pan) - get window associated with panel
--------------------------------------------------------------------------*/
WINDOW *
panel_window(const PANEL *pan)
{
  return(pan ? pan->win : (WINDOW *)0);
} /* end of panel_window */

/*+-------------------------------------------------------------------------
	update_panels() - wnoutrefresh windows in an orderly fashion
--------------------------------------------------------------------------*/
void
update_panels(void)
{
  PANEL *pan;

  dBug(("--> update_panels"));
  pan = __bottom_panel;
  while(pan)
    {
      __override(pan,P_UPDATE);
      pan = pan->above;
    }

  pan = __bottom_panel;
  while (pan)
    {
      if(is_wintouched(pan->win))
	Wnoutrefresh(pan);
      pan = pan->above;
    }
} /* end of update_panels */

/*+-------------------------------------------------------------------------
	hide_panel(pan) - remove a panel from stack
--------------------------------------------------------------------------*/
int
hide_panel(register PANEL *pan)
{
  if(!pan)
    return(ERR);

  dBug(("--> hide_panel %s", USER_PTR(pan->user)));

  if(!__panel_is_linked(pan))
    {
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      return(ERR);
    }

  __panel_unlink(pan);
  return(OK);
} /* end of hide_panel */

/*+-------------------------------------------------------------------------
	show_panel(pan) - place a panel on top of stack
may already be in stack
--------------------------------------------------------------------------*/
int
show_panel(register PANEL *pan)
{
  if(!pan)
    return(ERR);
  if(pan == __top_panel)
    return(OK);
  dBug(("--> show_panel %s", USER_PTR(pan->user)));
  if(__panel_is_linked(pan))
    (void)hide_panel(pan);
  __panel_link_top(pan);
  return(OK);
} /* end of show_panel */

/*+-------------------------------------------------------------------------
	top_panel(pan) - place a panel on top of stack
--------------------------------------------------------------------------*/
int
top_panel(register PANEL *pan)
{
  return(show_panel(pan));
} /* end of top_panel */

/*+-------------------------------------------------------------------------
	del_panel(pan) - remove a panel from stack, if in it, and free struct
--------------------------------------------------------------------------*/
int
del_panel(register PANEL *pan)
{
  if(pan)
    {
      dBug(("--> del_panel %s", USER_PTR(pan->user)));
      if(__panel_is_linked(pan))
	(void)hide_panel(pan);
      free((void *)pan);
      return(OK);
    }
  return(ERR);
} /* end of del_panel */

/*+-------------------------------------------------------------------------
	bottom_panel(pan) - place a panel on bottom of stack
may already be in stack
--------------------------------------------------------------------------*/
int
bottom_panel(register PANEL *pan)
{
  if(!pan)
    return(ERR);
  if(pan == __bottom_panel)
    return(OK);
  dBug(("--> bottom_panel %s", USER_PTR(pan->user)));
  if(__panel_is_linked(pan))
    (void)hide_panel(pan);
  __panel_link_bottom(pan);
  return(OK);
} /* end of bottom_panel */

/*+-------------------------------------------------------------------------
	new_panel(win) - create a panel and place on top of stack
--------------------------------------------------------------------------*/
PANEL *
new_panel(WINDOW *win)
{
  PANEL *pan = (PANEL *)malloc(sizeof(PANEL));

  (void)__root_panel();

  if(pan)
    {
      pan->win = win;
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      getbegyx(win, pan->wstarty, pan->wstartx);
      pan->wendy = pan->wstarty + getmaxy(win);
      pan->wendx = pan->wstartx + getmaxx(win);
#ifdef TRACE
      pan->user = "new";
#else
      pan->user = (char *)0;
#endif
      pan->obscure = (PANELCONS *)0;
      (void)show_panel(pan);
    }
  return(pan);
} /* end of new_panel */

/*+-------------------------------------------------------------------------
	panel_above(pan)
--------------------------------------------------------------------------*/
PANEL *
panel_above(const PANEL *pan)
{
  if(!pan)
    {
      /* if top and bottom are equal, we have no or only the pseudo panel;
	 if not, we return the panel above the pseudo panel */
      return(__bottom_panel==__top_panel ? (PANEL*)0 : __bottom_panel->above);
    }
  else
    return(pan->above);
} /* end of panel_above */

/*+-------------------------------------------------------------------------
	panel_below(pan)
--------------------------------------------------------------------------*/
PANEL *
panel_below(const PANEL *pan)
{
  if(!pan)
    {
      /* if top and bottom are equal, we have no or only the pseudo panel */
      return(__top_panel==__bottom_panel ? (PANEL*)0 : __top_panel);
    }
  else
    {
      /* we must not return the pseudo panel */
      return(pan->below==__bottom_panel ? (PANEL*) 0 : pan->below);
    }
} /* end of panel_below */

/*+-------------------------------------------------------------------------
	set_panel_userptr(pan,uptr)
--------------------------------------------------------------------------*/
int
set_panel_userptr(PANEL *pan, const void *uptr)
{
  if(!pan)
    return(ERR);
  pan->user = uptr;
  return(OK);
} /* end of set_panel_userptr */

/*+-------------------------------------------------------------------------
	panel_userptr(pan)
--------------------------------------------------------------------------*/
const void*
panel_userptr(const PANEL *pan)
{
  return(pan ? pan->user : (void *)0);
} /* end of panel_userptr */

/*+-------------------------------------------------------------------------
	move_panel(pan,starty,startx)
--------------------------------------------------------------------------*/
int
move_panel(PANEL *pan, int starty, int startx)
{
  WINDOW *win;

  if(!pan)
    return(ERR);
  if(__panel_is_linked(pan))
    __override(pan,P_TOUCH);
  win = pan->win;
  if(mvwin(win,starty,startx))
    return(ERR);
  getbegyx(win, pan->wstarty, pan->wstartx);
  pan->wendy = pan->wstarty + getmaxy(win);
  pan->wendx = pan->wstartx + getmaxx(win);
  if(__panel_is_linked(pan))
    __calculate_obscure();
  return(OK);
} /* end of move_panel */

/*+-------------------------------------------------------------------------
	replace_panel(pan,win)
--------------------------------------------------------------------------*/
int
replace_panel(PANEL *pan, WINDOW *win)
{
  if(!pan)
    return(ERR);
  if(__panel_is_linked(pan))
    __override(pan,P_TOUCH);
  pan->win = win;
  if(__panel_is_linked(pan))
    __calculate_obscure();
  return(OK);
} /* end of replace_panel */

/*+-------------------------------------------------------------------------
	panel_hidden(pan)
--------------------------------------------------------------------------*/
int
panel_hidden(const PANEL *pan)
{
  if(!pan)
    return(ERR);
  return(__panel_is_linked(pan) ? TRUE : FALSE);
} /* end of panel_hidden */

/* end of panel.c */
