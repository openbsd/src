/*	$OpenBSD: basic.c,v 1.21 2005/11/18 20:56:52 deraadt Exp $	*/

/* This file is in the public domain */

/*
 *		Basic cursor motion commands.
 *
 * The routines in this file are the basic
 * command functions for moving the cursor around on
 * the screen, setting mark, and swapping dot with
 * mark. Only moves between lines, which might make the
 * current buffer framing bad, are hard.
 */
#include "def.h"

#include <ctype.h>

/*
 * Go to beginning of line.
 */
/* ARGSUSED */
int
gotobol(int f, int n)
{
	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * Move cursor backwards. Do the
 * right thing if the count is less than
 * 0. Error if you try to move back from
 * the beginning of the buffer.
 */
/* ARGSUSED */
int
backchar(int f, int n)
{
	struct line   *lp;

	if (n < 0)
		return (forwchar(f, -n));
	while (n--) {
		if (curwp->w_doto == 0) {
			if ((lp = lback(curwp->w_dotp)) == curbp->b_linep) {
				if (!(f & FFRAND))
					ewprintf("Beginning of buffer");
				return (FALSE);
			}
			curwp->w_dotp = lp;
			curwp->w_doto = llength(lp);
			curwp->w_flag |= WFMOVE;
		} else
			curwp->w_doto--;
	}
	return (TRUE);
}

/*
 * Go to end of line.
 */
/* ARGSUSED */
int
gotoeol(int f, int n)
{
	curwp->w_doto = llength(curwp->w_dotp);
	return (TRUE);
}

/*
 * Move cursor forwards. Do the
 * right thing if the count is less than
 * 0. Error if you try to move forward
 * from the end of the buffer.
 */
/* ARGSUSED */
int
forwchar(int f, int n)
{
	if (n < 0)
		return (backchar(f, -n));
	while (n--) {
		if (curwp->w_doto == llength(curwp->w_dotp)) {
			curwp->w_dotp = lforw(curwp->w_dotp);
			if (curwp->w_dotp == curbp->b_linep) {
				curwp->w_dotp = lback(curwp->w_dotp);
				if (!(f & FFRAND))
					ewprintf("End of buffer");
				return (FALSE);
			}
			curwp->w_doto = 0;
			curwp->w_flag |= WFMOVE;
		} else
			curwp->w_doto++;
	}
	return (TRUE);
}

/*
 * Go to the beginning of the
 * buffer. Setting WFHARD is conservative,
 * but almost always the case.
 */
int
gotobob(int f, int n)
{
	(void) setmark(f, n);
	curwp->w_dotp = lforw(curbp->b_linep);
	curwp->w_doto = 0;
	curwp->w_flag |= WFHARD;
	return (TRUE);
}

/*
 * Go to the end of the buffer.
 * Setting WFHARD is conservative, but
 * almost always the case.
 */
int
gotoeob(int f, int n)
{
	(void) setmark(f, n);
	curwp->w_dotp = lback(curbp->b_linep);
	curwp->w_doto = llength(curwp->w_dotp);
	curwp->w_flag |= WFHARD;
	return (TRUE);
}

/*
 * Move forward by full lines.
 * If the number of lines to move is less
 * than zero, call the backward line function to
 * actually do it. The last command controls how
 * the goal column is set.
 */
/* ARGSUSED */
int
forwline(int f, int n)
{
	struct line  *dlp;

	if (n < 0)
		return (backline(f | FFRAND, -n));
	if ((lastflag & CFCPCN) == 0)	/* Fix goal. */
		setgoal();
	thisflag |= CFCPCN;
	if (n == 0)
		return (TRUE);
	dlp = curwp->w_dotp;
	while (dlp != curbp->b_linep && n--)
		dlp = lforw(dlp);
	curwp->w_flag |= WFMOVE;
	if (dlp == curbp->b_linep) {	/* ^N at end of buffer creates lines
					 * (like gnu) */
		if (!(curbp->b_flag & BFCHG)) {	/* first change */
			curbp->b_flag |= BFCHG;
			curwp->w_flag |= WFMODE;
		}
		curwp->w_doto = 0;
		while (n-- >= 0) {
			if ((dlp = lalloc(0)) == NULL)
				return (FALSE);
			dlp->l_fp = curbp->b_linep;
			dlp->l_bp = lback(dlp->l_fp);
			dlp->l_bp->l_fp = dlp->l_fp->l_bp = dlp;
		}
		curwp->w_dotp = lback(curbp->b_linep);
	} else {
		curwp->w_dotp = dlp;
		curwp->w_doto = getgoal(dlp);
	}
	return (TRUE);
}

/*
 * This function is like "forwline", but
 * goes backwards. The scheme is exactly the same.
 * Check for arguments that are less than zero and
 * call your alternate. Figure out the new line and
 * call "movedot" to perform the motion.
 */
/* ARGSUSED */
int
backline(int f, int n)
{
	struct line   *dlp;

	if (n < 0)
		return (forwline(f | FFRAND, -n));
	if ((lastflag & CFCPCN) == 0)	/* Fix goal. */
		setgoal();
	thisflag |= CFCPCN;
	dlp = curwp->w_dotp;
	while (n-- && lback(dlp) != curbp->b_linep)
		dlp = lback(dlp);
	curwp->w_dotp = dlp;
	curwp->w_doto = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	return (TRUE);
}

/*
 * Set the current goal column, which is saved in the external variable
 * "curgoal", to the current cursor column. The column is never off
 * the edge of the screen; it's more like display then show position.
 */
void
setgoal(void)
{
	curgoal = getcolpos();		/* Get the position. */
	/* we can now display past end of display, don't chop! */
}

/*
 * This routine looks at a line (pointed
 * to by the LINE pointer "dlp") and the current
 * vertical motion goal column (set by the "setgoal"
 * routine above) and returns the best offset to use
 * when a vertical motion is made into the line.
 */
int
getgoal(struct line *dlp)
{
	int c, i, col = 0;

	for (i = 0; i < llength(dlp); i++) {
		c = lgetc(dlp, i);
		if (c == '\t'
#ifdef	NOTAB
		    && !(curbp->b_flag & BFNOTAB)
#endif
			) {
			col |= 0x07;
			col++;
		} else if (ISCTRL(c) != FALSE) {
			col += 2;
		} else if (isprint(c))
			col++;
		else {
			char tmp[5];

			snprintf(tmp, sizeof(tmp), "\\%o", c);
			col += strlen(tmp);
		}
		if (col > curgoal)
			break;
	}
	return (i);
}

/*
 * Scroll forward by a specified number
 * of lines, or by a full page if no argument.
 * The "2" is the window overlap (this is the default
 * value from ITS EMACS). Because the top line in
 * the window is zapped, we have to do a hard
 * update and get it back.
 */
/* ARGSUSED */
int
forwpage(int f, int n)
{
	struct line  *lp;

	if (!(f & FFARG)) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	 */
		if (n <= 0)			/* Forget the overlap	 */
			n = 1;			/* if tiny window.	 */
	} else if (n < 0)
		return (backpage(f | FFRAND, -n));
#ifdef	CVMVAS
	else					/* Convert from pages	 */
		n *= curwp->w_ntrows;		/* to lines.		 */
#endif
	lp = curwp->w_linep;
	while (n-- && lforw(lp) != curbp->b_linep)
		lp = lforw(lp);
	curwp->w_linep = lp;
	curwp->w_flag |= WFHARD;
	/* if in current window, don't move dot */
	for (n = curwp->w_ntrows; n-- && lp != curbp->b_linep; lp = lforw(lp))
		if (lp == curwp->w_dotp)
			return (TRUE);
	curwp->w_dotp = curwp->w_linep;
	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * This command is like "forwpage",
 * but it goes backwards. The "2", like above,
 * is the overlap between the two windows. The
 * value is from the ITS EMACS manual. The
 * hard update is done because the top line in
 * the window is zapped.
 */
/* ARGSUSED */
int
backpage(int f, int n)
{
	struct line  *lp;

	if (!(f & FFARG)) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	 */
		if (n <= 0)			/* Don't blow up if the  */
			n = 1;			/* window is tiny.	 */
	} else if (n < 0)
		return (forwpage(f | FFRAND, -n));
#ifdef	CVMVAS
	else					/* Convert from pages	 */
		n *= curwp->w_ntrows;		/* to lines.		 */
#endif
	lp = curwp->w_linep;
	while (n-- && lback(lp) != curbp->b_linep)
		lp = lback(lp);
	curwp->w_linep = lp;
	curwp->w_flag |= WFHARD;
	/* if in current window, don't move dot */
	for (n = curwp->w_ntrows; n-- && lp != curbp->b_linep; lp = lforw(lp))
		if (lp == curwp->w_dotp)
			return (TRUE);
	curwp->w_dotp = curwp->w_linep;
	curwp->w_doto = 0;
	return (TRUE);
}

/*
 * These functions are provided for compatibility with Gosling's Emacs. They
 * are used to scroll the display up (or down) one line at a time.
 */
int
forw1page(int f, int n)
{
	if (!(f & FFARG)) {
		n = 1;
		f = FFUNIV;
	}
	forwpage(f | FFRAND, n);
	return (TRUE);
}

int
back1page(int f, int n)
{
	if (!(f & FFARG)) {
		n = 1;
		f = FFUNIV;
	}
	backpage(f | FFRAND, n);
	return (TRUE);
}

/*
 * Page the other window. Check to make sure it exists, then
 * nextwind, forwpage and restore window pointers.
 */
int
pagenext(int f, int n)
{
	struct mgwin *wp;

	if (wheadp->w_wndp == NULL) {
		ewprintf("No other window");
		return (FALSE);
	}
	wp = curwp;
	(void) nextwind(f, n);
	(void) forwpage(f, n);
	curwp = wp;
	curbp = wp->w_bufp;
	return (TRUE);
}

/*
 * Internal set mark routine, used by other functions (daveb).
 */
void
isetmark(void)
{
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
}

/*
 * Set the mark in the current window
 * to the value of dot. A message is written to
 * the echo line.  (ewprintf knows about macros)
 */
/* ARGSUSED */
int
setmark(int f, int n)
{
	isetmark();
	ewprintf("Mark set");
	return (TRUE);
}

/*
 * Swap the values of "dot" and "mark" in
 * the current window. This is pretty easy, because
 * all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible
 * error is "no mark".
 */
/* ARGSUSED */
int
swapmark(int f, int n)
{
	struct line  *odotp;
	int    odoto;

	if (curwp->w_markp == NULL) {
		ewprintf("No mark in this window");
		return (FALSE);
	}
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	curwp->w_dotp = curwp->w_markp;
	curwp->w_doto = curwp->w_marko;
	curwp->w_markp = odotp;
	curwp->w_marko = odoto;
	curwp->w_flag |= WFMOVE;
	return (TRUE);
}

/*
 * Go to a specific line, mostly for
 * looking up errors in C programs, which give the
 * error a line number. If an argument is present, then
 * it is the line number, else prompt for a line number
 * to use.
 */
/* ARGSUSED */
int
gotoline(int f, int n)
{
	struct line  *clp;
	char   buf[32], *bufp, *tmp;
	long   nl;

	if (!(f & FFARG)) {
		if ((bufp = eread("Goto line: ", buf, sizeof(buf),
		    EFNUL | EFNEW | EFCR)) == NULL)
			return (ABORT);
		nl = strtol(bufp, &tmp, 10);
		if (bufp[0] == '\0' || *tmp != '\0') {
			ewprintf("Invalid number");
			return (FALSE);
		}
		if (nl >= INT_MAX || nl <= INT_MIN) {
			ewprintf("Out of range");
			return (FALSE);
		}
		n = (int)nl;
	}
	if (n >= 0) {
		clp = lforw(curbp->b_linep);	/* "clp" is first line */
		while (--n > 0) {
			if (lforw(clp) == curbp->b_linep)
				break;
			clp = lforw(clp);
		}
	} else {
		clp = lback(curbp->b_linep);	/* "clp" is last line */
		while (n < 0) {
			if (lback(clp) == curbp->b_linep)
				break;
			clp = lback(clp);
			n++;
		}
	}
	curwp->w_dotp = clp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return (TRUE);
}
