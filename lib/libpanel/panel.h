
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

/* panel.h -- interface file for panels library */

#ifndef _PANEL_H
#define _PANEL_H

#include <curses.h>

typedef struct panel
{
	WINDOW *win;
	int wstarty;
	int wendy;
	int wstartx;
	int wendx;
	struct panel *below;
	struct panel *above;
	char *user;
	struct panelcons *obscure;
}
PANEL;

#if	defined(__cplusplus)
extern "C" {
#endif

extern  WINDOW *panel_window(PANEL *);
extern  void update_panels(void);
extern  int hide_panel(PANEL *);
extern  int show_panel(PANEL *);
extern  int del_panel(PANEL *);
extern  int top_panel(PANEL *);
extern  int bottom_panel(PANEL *);
extern  PANEL *new_panel(WINDOW *);
extern  PANEL *panel_above(PANEL *);
extern  PANEL *panel_below(PANEL *);
extern  int set_panel_userptr(PANEL *,char *);
extern  char *panel_userptr(PANEL *);
extern  int move_panel(PANEL *, int, int);
extern  int replace_panel(PANEL *,WINDOW *);
extern	int panel_hidden(PANEL *);

#if	defined(__cplusplus)
}
#endif

#endif /* _PANEL_H */

/* end of panel.h */
