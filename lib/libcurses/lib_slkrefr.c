/*	$OpenBSD: lib_slkrefr.c,v 1.1 1997/12/03 05:21:34 millert Exp $	*/


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
 *	lib_slkrefr.c
 *	Write SLK window to the (virtual) screen.
 */
#include <curses.priv.h>
#include <term.h>	  /* num_labels, label_*, plab_norm */

MODULE_ID("Id: lib_slkrefr.c,v 1.3 1997/10/18 19:02:17 tom Exp $")

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
					if (SP && SP->_slk)
					  wattrset(slk->win,SP->_slk->attr);
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
	T((T_CALLED("slk_noutrefresh()")));

	if (SP == NULL || SP->_slk == NULL)
		returnCode(ERR);
	if (SP->_slk->hidden)
		returnCode(OK);
	slk_intern_refresh(SP->_slk);

	returnCode(wnoutrefresh(SP->_slk->win));
}

/*
 * Refresh the soft labels.
 */
int
slk_refresh(void)
{
	T((T_CALLED("slk_refresh()")));

	if (SP == NULL || SP->_slk == NULL)
		returnCode(ERR);
	if (SP->_slk->hidden)
		returnCode(OK);
	slk_intern_refresh(SP->_slk);

	returnCode(wrefresh(SP->_slk->win));
}
