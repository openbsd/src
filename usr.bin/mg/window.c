/*
 *		Window handling.
 */
#include	"def.h"

/*
 * Reposition dot in the current
 * window to line "n". If the argument is
 * positive, it is that line. If it is negative it
 * is that line from the bottom. If it is 0 the window
 * is centered (this is what the standard redisplay code
 * does).  If GOSREC is undefined, default is 0, so it acts like GNU.
 * If GOSREC is defined, with no argument it defaults to 1
 * and works like in Gosling.
 */
/*ARGSUSED*/
reposition(f, n)
{
#ifndef GOSREC
	curwp->w_force = (f & FFARG) ? (n>=0 ? n+1 : n) : 0;
#else
	curwp->w_force = n;
#endif
	curwp->w_flag |= WFFORCE;
	sgarbf = TRUE;
	return TRUE;
}

/*
 * Refresh the display. A call is made to the
 * "ttresize" entry in the terminal handler, which tries
 * to reset "nrow" and "ncol". They will, however, never
 * be set outside of the NROW or NCOL range. If the display
 * changed size, arrange that everything is redone, then
 * call "update" to fix the display. We do this so the
 * new size can be displayed. In the normal case the
 * call to "update" in "main.c" refreshes the screen,
 * and all of the windows need not be recomputed.
 * Note that when you get to the "display unusable"
 * message, the screen will be messed up. If you make
 * the window bigger again, and send another command,
 * everything will get fixed!
 */
/*ARGSUSED*/
refresh(f, n)
{
	register MGWIN *wp;
	register int	oldnrow;
	register int	oldncol;

	oldnrow = nrow;
	oldncol = ncol;
	ttresize();
	if (nrow!=oldnrow || ncol!=oldncol) {
		wp = wheadp;			/* Find last.		*/
		while (wp->w_wndp != NULL)
			wp = wp->w_wndp;
		if (nrow < wp->w_toprow+3) {	/* Check if too small.	*/
			ewprintf("Display unusable");
			return (FALSE);
		}
		wp->w_ntrows = nrow-wp->w_toprow-2;
		sgarbf = TRUE;
		update();
		ewprintf("New size %d by %d", nrow, ncol);
	} else
		sgarbf = TRUE;
	return TRUE;
}

/*
 * The command to make the next
 * window (next => down the screen)
 * the current window. There are no real
 * errors, although the command does
 * nothing if there is only 1 window on
 * the screen.
 */
/*ARGSUSED*/
nextwind(f, n)
{
	register MGWIN *wp;

	if ((wp=curwp->w_wndp) == NULL)
		wp = wheadp;
	curwp = wp;
	curbp = wp->w_bufp;
	return TRUE;
}

#ifdef	GOSMACS
/* not in Gnu Emacs */
/*
 * This command makes the previous
 * window (previous => up the screen) the
 * current window. There arn't any errors,
 * although the command does not do a lot
 * if there is 1 window.
 */
/*ARGSUSED*/
prevwind(f, n)
{
	register MGWIN *wp1;
	register MGWIN *wp2;

	wp1 = wheadp;
	wp2 = curwp;
	if (wp1 == wp2)
		wp2 = NULL;
	while (wp1->w_wndp != wp2)
		wp1 = wp1->w_wndp;
	curwp = wp1;
	curbp = wp1->w_bufp;
	return TRUE;
}
#endif

/*
 * This command makes the current
 * window the only window on the screen.
 * Try to set the framing
 * so that "." does not have to move on
 * the display. Some care has to be taken
 * to keep the values of dot and mark
 * in the buffer structures right if the
 * distruction of a window makes a buffer
 * become undisplayed.
 */
/*ARGSUSED*/
onlywind(f, n)
{
	register MGWIN *wp;
	register LINE	*lp;
	register int	i;

	while (wheadp != curwp) {
		wp = wheadp;
		wheadp = wp->w_wndp;
		if (--wp->w_bufp->b_nwnd == 0) {
			wp->w_bufp->b_dotp  = wp->w_dotp;
			wp->w_bufp->b_doto  = wp->w_doto;
			wp->w_bufp->b_markp = wp->w_markp;
			wp->w_bufp->b_marko = wp->w_marko;
		}
		free((char *) wp);
	}
	while (curwp->w_wndp != NULL) {
		wp = curwp->w_wndp;
		curwp->w_wndp = wp->w_wndp;
		if (--wp->w_bufp->b_nwnd == 0) {
			wp->w_bufp->b_dotp  = wp->w_dotp;
			wp->w_bufp->b_doto  = wp->w_doto;
			wp->w_bufp->b_markp = wp->w_markp;
			wp->w_bufp->b_marko = wp->w_marko;
		}
		free((char *) wp);
	}
	lp = curwp->w_linep;
	i  = curwp->w_toprow;
	while (i!=0 && lback(lp)!=curbp->b_linep) {
		--i;
		lp = lback(lp);
	}
	curwp->w_toprow = 0;
	curwp->w_ntrows = nrow-2;		/* 2 = mode, echo.	*/
	curwp->w_linep	= lp;
	curwp->w_flag  |= WFMODE|WFHARD;
	return TRUE;
}

/*
 * Split the current window. A window
 * smaller than 3 lines cannot be split.
 * The only other error that is possible is
 * a "malloc" failure allocating the structure
 * for the new window.
 */
/*ARGSUSED*/
splitwind(f, n)
{
	register MGWIN *wp;
	register LINE	*lp;
	register int	ntru;
	register int	ntrd;
	int		ntrl;
	MGWIN		*wp1, *wp2;

	if (curwp->w_ntrows < 3) {
		ewprintf("Cannot split a %d line window", curwp->w_ntrows);
		return (FALSE);
	}
	if ((wp = (MGWIN *)malloc(sizeof(MGWIN))) == NULL) {
		ewprintf("Can't get %d", sizeof(MGWIN));
		return (FALSE);
	}
	++curbp->b_nwnd;			/* Displayed twice.	*/
	wp->w_bufp  = curbp;
	wp->w_dotp  = curwp->w_dotp;
	wp->w_doto  = curwp->w_doto;
	wp->w_markp = curwp->w_markp;
	wp->w_marko = curwp->w_marko;
	wp->w_flag  = 0;
	wp->w_force = 0;
	ntru = (curwp->w_ntrows-1) / 2;		/* Upper size		*/
	ntrl = (curwp->w_ntrows-1) - ntru;	/* Lower size		*/
	lp = curwp->w_linep;
	ntrd = 0;
	while (lp != curwp->w_dotp) {
		++ntrd;
		lp = lforw(lp);
	}
	lp = curwp->w_linep;
	if (ntrd <= ntru) {			/* Old is upper window. */
		if (ntrd == ntru)		/* Hit mode line.	*/
			lp = lforw(lp);
		curwp->w_ntrows = ntru;
		wp->w_wndp = curwp->w_wndp;
		curwp->w_wndp = wp;
		wp->w_toprow = curwp->w_toprow+ntru+1;
		wp->w_ntrows = ntrl;
	} else {				/* Old is lower window	*/
		wp1 = NULL;
		wp2 = wheadp;
		while (wp2 != curwp) {
			wp1 = wp2;
			wp2 = wp2->w_wndp;
		}
		if (wp1 == NULL)
			wheadp = wp;
		else
			wp1->w_wndp = wp;
		wp->w_wndp   = curwp;
		wp->w_toprow = curwp->w_toprow;
		wp->w_ntrows = ntru;
		++ntru;				/* Mode line.		*/
		curwp->w_toprow += ntru;
		curwp->w_ntrows	 = ntrl;
		while (ntru--)
			lp = lforw(lp);
	}
	curwp->w_linep = lp;			/* Adjust the top lines */
	wp->w_linep = lp;			/* if necessary.	*/
	curwp->w_flag |= WFMODE|WFHARD;
	wp->w_flag |= WFMODE|WFHARD;
	return TRUE;
}

/*
 * Enlarge the current window.
 * Find the window that loses space. Make
 * sure it is big enough. If so, hack the window
 * descriptions, and ask redisplay to do all the
 * hard work. You don't just set "force reframe"
 * because dot would move.
 */
/*ARGSUSED*/
enlargewind(f, n)
{
	register MGWIN *adjwp;
	register LINE	*lp;
	register int	i;

	if (n < 0)
		return shrinkwind(f, -n);
	if (wheadp->w_wndp == NULL) {
		ewprintf("Only one window");
		return FALSE;
	}
	if ((adjwp=curwp->w_wndp) == NULL) {
		adjwp = wheadp;
		while (adjwp->w_wndp != curwp)
			adjwp = adjwp->w_wndp;
	}
	if (adjwp->w_ntrows <= n) {
		ewprintf("Impossible change");
		return FALSE;
	}
	if (curwp->w_wndp == adjwp) {		/* Shrink below.	*/
		lp = adjwp->w_linep;
		for (i=0; i<n && lp!=adjwp->w_bufp->b_linep; ++i)
			lp = lforw(lp);
		adjwp->w_linep	= lp;
		adjwp->w_toprow += n;
	} else {				/* Shrink above.	*/
		lp = curwp->w_linep;
		for (i=0; i<n && lback(lp)!=curbp->b_linep; ++i)
			lp = lback(lp);
		curwp->w_linep	= lp;
		curwp->w_toprow -= n;
	}
	curwp->w_ntrows += n;
	adjwp->w_ntrows -= n;
	curwp->w_flag |= WFMODE|WFHARD;
	adjwp->w_flag |= WFMODE|WFHARD;
	return TRUE;
}

/*
 * Shrink the current window.
 * Find the window that gains space. Hack at
 * the window descriptions. Ask the redisplay to
 * do all the hard work.
 */
shrinkwind(f, n)
{
	register MGWIN *adjwp;
	register LINE	*lp;
	register int	i;

	if (n < 0)
		return enlargewind(f, -n);
	if (wheadp->w_wndp == NULL) {
		ewprintf("Only one window");
		return FALSE;
	}
	/*
	 * Bit of flakiness - KRANDOM means it was an internal call, and
	 * to be trusted implicitly about sizes.
	 */
	if ( !(f & FFRAND) && curwp->w_ntrows <= n) {
		ewprintf("Impossible change");
		return (FALSE);
	}
	if ((adjwp=curwp->w_wndp) == NULL) {
		adjwp = wheadp;
		while (adjwp->w_wndp != curwp)
			adjwp = adjwp->w_wndp;
	}
	if (curwp->w_wndp == adjwp) {		/* Grow below.		*/
		lp = adjwp->w_linep;
		for (i=0; i<n && lback(lp)!=adjwp->w_bufp->b_linep; ++i)
			lp = lback(lp);
		adjwp->w_linep	= lp;
		adjwp->w_toprow -= n;
	} else {				/* Grow above.		*/
		lp = curwp->w_linep;
		for (i=0; i<n && lp!=curbp->b_linep; ++i)
			lp = lforw(lp);
		curwp->w_linep	= lp;
		curwp->w_toprow += n;
	}
	curwp->w_ntrows -= n;
	adjwp->w_ntrows += n;
	curwp->w_flag |= WFMODE|WFHARD;
	adjwp->w_flag |= WFMODE|WFHARD;
	return (TRUE);
}

/*
 * Delete current window. Call shrink-window to do the screen
 * updating, then throw away the window.
 */
/*ARGSUSED*/
delwind(f, n)
{
	register MGWIN *wp, *nwp;

	wp = curwp;			/* Cheap...		*/
	/* shrinkwind returning false means only one window... */
	if (shrinkwind(FFRAND, wp->w_ntrows + 1) == FALSE)
		return FALSE;
	if (--wp->w_bufp->b_nwnd == 0) {
		wp->w_bufp->b_dotp  = wp->w_dotp;
		wp->w_bufp->b_doto  = wp->w_doto;
		wp->w_bufp->b_markp = wp->w_markp;
		wp->w_bufp->b_marko = wp->w_marko;
	}
	/* since shrinkwind did't crap out, we know we have a second window */
	if (wp == wheadp) wheadp = curwp = wp->w_wndp;
	else if ((curwp = wp->w_wndp) == NULL) curwp = wheadp;
	curbp = curwp->w_bufp;
	for (nwp = wheadp; nwp != NULL; nwp = nwp->w_wndp)
		if (nwp->w_wndp == wp) {
			nwp->w_wndp = wp->w_wndp;
			break ;
		}
	free((char *) wp);
	return TRUE;
}
/*
 * Pick a window for a pop-up.
 * Split the screen if there is only
 * one window. Pick the uppermost window that
 * isn't the current window. An LRU algorithm
 * might be better. Return a pointer, or
 * NULL on error.
 */
MGWIN	*
wpopup() {
	register MGWIN *wp;

	if (wheadp->w_wndp == NULL
	&& splitwind(FFRAND, 0) == FALSE)
		return NULL;
	wp = wheadp;				/* Find window to use	*/
	while (wp!=NULL && wp==curwp)
		wp = wp->w_wndp;
	return wp;
}
