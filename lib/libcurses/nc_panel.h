/*	$OpenBSD: nc_panel.h,v 1.1 1997/12/03 05:21:42 millert Exp $	*/


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
 * Id: nc_panel.h,v 1.1 1997/10/21 10:00:34 juergen Exp $
 *
 *	nc_panel.h
 *
 *	Headerfile to provide an interface for the panel layer into
 *      the SCREEN structure of the ncurses core.
 */

#ifndef NC_PANEL_H
#define NC_PANEL_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct panel; /* Forward Declaration */

struct panelhook {
  struct panel*   top_panel;
  struct panel*   bottom_panel;
  struct panel*   stdscr_pseudo_panel;
};

/* Retrieve the panelhook of the current screen */
extern struct panelhook* _nc_panelhook(void);

#ifdef __cplusplus
}
#endif

#endif /* NC_PANEL_H */
