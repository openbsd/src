/*	$OpenBSD: random.c,v 1.27 2011/01/17 03:12:06 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *		Assorted commands.
 * This file contains the command processors for a large assortment of
 * unrelated commands.  The only thing they have in common is that they
 * are all command processors.
 */

#include "def.h"
#include <ctype.h>

/*
 * Display a bunch of useful information about the current location of dot.
 * The character under the cursor (in octal), the current line, row, and
 * column, and approximate position of the cursor in the file (as a
 * percentage) is displayed.  The column position assumes an infinite
 * position display; it does not truncate just because the screen does.
 * This is normally bound to "C-X =".
 */
/* ARGSUSED */
int
showcpos(int f, int n)
{
	struct line	*clp;
	long	 nchar, cchar;
	int	 nline, row;
	int	 cline, cbyte;		/* Current line/char/byte */
	int	 ratio;

	/* collect the data */
	clp = bfirstlp(curbp);
	cchar = 0;
	cline = 0;
	cbyte = 0;
	nchar = 0;
	nline = 0;
	for (;;) {
		/* count this line */
		++nline;
		if (clp == curwp->w_dotp) {
			/* mark line */
			cline = nline;
			cchar = nchar + curwp->w_doto;
			if (curwp->w_doto == llength(clp))
				cbyte = '\n';
			else
				cbyte = lgetc(clp, curwp->w_doto);
		}
		/* now count the chars */
		nchar += llength(clp);
		clp = lforw(clp);
		if (clp == curbp->b_headp)
			break;
		/* count the newline */
		nchar++;
	}
	/* determine row */
	row = curwp->w_toprow + 1;
	clp = curwp->w_linep;
	while (clp != curbp->b_headp && clp != curwp->w_dotp) {
		++row;
		clp = lforw(clp);
	}
	/* NOSTRICT */
	ratio = nchar ? (100L * cchar) / nchar : 100;
	ewprintf("Char: %c (0%o)  point=%ld(%d%%)  line=%d  row=%d  col=%d",
	    cbyte, cbyte, cchar, ratio, cline, row, getcolpos());
	return (TRUE);
}

int
getcolpos(void)
{
	int	col, i, c;
	char tmp[5];

	/* determine column */
	col = 0;

	for (i = 0; i < curwp->w_doto; ++i) {
		c = lgetc(curwp->w_dotp, i);
		if (c == '\t'
#ifdef NOTAB
		    && !(curbp->b_flag & BFNOTAB)
#endif /* NOTAB */
			) {
			col |= 0x07;
			col++;
		} else if (ISCTRL(c) != FALSE)
			col += 2;
		else if (isprint(c)) {
			col++;
		} else {
			col += snprintf(tmp, sizeof(tmp), "\\%o", c);
		}

	}
	return (col);
}

/*
 * Twiddle the two characters on either side of dot.  If dot is at the end
 * of the line twiddle the two characters before it.  Return with an error
 * if dot is at the beginning of line; it seems to be a bit pointless to
 * make this work.  This fixes up a very common typo with a single stroke.
 * Normally bound to "C-T".  This always works within a line, so "WFEDIT"
 * is good enough.
 */
/* ARGSUSED */
int
twiddle(int f, int n)
{
	struct line	*dotp;
	int	 doto, cr;
	int	 fudge = FALSE;

	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	undo_boundary_enable(FFRAND, 0);
	if (doto == llength(dotp)) {
		if (--doto <= 0)
			return (FALSE);
		(void)backchar(FFRAND, 1);
		fudge = TRUE;
	} else {
		if (doto == 0)
			return (FALSE);
	}
	cr = lgetc(dotp, doto - 1);
	(void)backdel(FFRAND, 1);
	(void)forwchar(FFRAND, 1);
	linsert(1, cr);
	if (fudge != TRUE)
		(void)backchar(FFRAND, 1);
	undo_boundary_enable(FFRAND, 1);
	lchange(WFEDIT);
	return (TRUE);
}

/*
 * Open up some blank space.  The basic plan is to insert a bunch of
 * newlines, and then back up over them.  Everything is done by the
 * subcommand processors.  They even handle the looping.  Normally this
 * is bound to "C-O".
 */
/* ARGSUSED */
int
openline(int f, int n)
{
	int	i, s;

	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);

	/* insert newlines */
	i = n;
	do {
		s = lnewline();
	} while (s == TRUE && --i);

	/* then go back up overtop of them all */
	if (s == TRUE)
		s = backchar(f | FFRAND, n);
	return (s);
}

/*
 * Insert a newline.
 */
/* ARGSUSED */
int
newline(int f, int n)
{
	int	 s;

	if (n < 0)
		return (FALSE);

	while (n--) {
		if ((s = lnewline()) != TRUE)
			return (s);
	}
	return (TRUE);
}

/*
 * Delete blank lines around dot. What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a blank line, this command
 * deletes all the blank lines above and below the current line. If it is
 * sitting on a non blank line then it deletes all of the blank lines after
 * the line. Normally this command is bound to "C-X C-O". Any argument is
 * ignored.
 */
/* ARGSUSED */
int
deblank(int f, int n)
{
	struct line	*lp1, *lp2;
	RSIZE	 nld;

	lp1 = curwp->w_dotp;
	while (llength(lp1) == 0 && (lp2 = lback(lp1)) != curbp->b_headp)
		lp1 = lp2;
	lp2 = lp1;
	nld = (RSIZE)0;
	while ((lp2 = lforw(lp2)) != curbp->b_headp && llength(lp2) == 0)
		++nld;
	if (nld == 0)
		return (TRUE);
	curwp->w_dotp = lforw(lp1);
	curwp->w_doto = 0;
	return (ldelete((RSIZE)nld, KNONE));
}

/*
 * Delete any whitespace around dot, then insert a space.
 */
int
justone(int f, int n)
{
	(void)delwhite(f, n);
	return (linsert(1, ' '));
}

/*
 * Delete any whitespace around dot.
 */
/* ARGSUSED */
int
delwhite(int f, int n)
{
	int	col, s;

	col = curwp->w_doto;

	while (col < llength(curwp->w_dotp) &&
	    (isspace(lgetc(curwp->w_dotp, col))))
		++col;
	do {
		if (curwp->w_doto == 0) {
			s = FALSE;
			break;
		}
		if ((s = backchar(FFRAND, 1)) != TRUE)
			break;
	} while (isspace(lgetc(curwp->w_dotp, curwp->w_doto)));

	if (s == TRUE)
		(void)forwchar(FFRAND, 1);
	(void)ldelete((RSIZE)(col - curwp->w_doto), KNONE);
	return (TRUE);
}

/*
 * Delete any leading whitespace on the current line
 */
int
delleadwhite(int f, int n)
{
	int soff, ls;
	struct line *slp;

	/* Save current position */
	slp = curwp->w_dotp;
	soff = curwp->w_doto;

	for (ls = 0; ls < llength(slp); ls++)
                 if (!isspace(lgetc(slp, ls)))
                        break;
	gotobol(FFRAND, 1);
	forwdel(FFRAND, ls);
	soff -= ls;
	if (soff < 0)
		soff = 0;
	forwchar(FFRAND, soff);

	return (TRUE);
}

/*
 * Delete any trailing whitespace on the current line
 */
int
deltrailwhite(int f, int n)
{
	int soff;

	/* Save current position */
	soff = curwp->w_doto;

	gotoeol(FFRAND, 1);
	delwhite(FFRAND, 1);

	/* restore original position, if possible */
	if (soff < curwp->w_doto)
		curwp->w_doto = soff;

	return (TRUE);
}



/*
 * Insert a newline, then enough tabs and spaces to duplicate the indentation
 * of the previous line.  Assumes tabs are every eight characters.  Quite
 * simple.  Figure out the indentation of the current line.  Insert a newline
 * by calling the standard routine.  Insert the indentation by inserting the
 * right number of tabs and spaces.  Return TRUE if all ok.  Return FALSE if
 * one of the subcommands failed. Normally bound to "C-M".
 */
/* ARGSUSED */
int
lfindent(int f, int n)
{
	int	c, i, nicol;

	if (n < 0)
		return (FALSE);

	while (n--) {
		nicol = 0;
		for (i = 0; i < llength(curwp->w_dotp); ++i) {
			c = lgetc(curwp->w_dotp, i);
			if (c != ' ' && c != '\t')
				break;
			if (c == '\t')
				nicol |= 0x07;
			++nicol;
		}
		if (lnewline() == FALSE || ((
#ifdef	NOTAB
		    curbp->b_flag & BFNOTAB) ? linsert(nicol, ' ') == FALSE : (
#endif /* NOTAB */
		    ((i = nicol / 8) != 0 && linsert(i, '\t') == FALSE) ||
		    ((i = nicol % 8) != 0 && linsert(i, ' ') == FALSE))))
			return (FALSE);
	}
	return (TRUE);
}

/*
 * Indent the current line. Delete existing leading whitespace,
 * and use tabs/spaces to achieve correct indentation. Try
 * to leave dot where it started.
 */
int
indent(int f, int n)
{
	int soff, i;

	if (n < 0)
		return (FALSE);

	delleadwhite(FFRAND, 1);

	/* If not invoked with a numerical argument, done */
	if (!(f & FFARG))
		return (TRUE);

	/* insert appropriate whitespace */
	soff = curwp->w_doto;
	(void)gotobol(FFRAND, 1);
	if (
#ifdef	NOTAB
	    curbp->b_flag & BFNOTAB) ? linsert(n, ' ') == FALSE :
#endif /* NOTAB */
	    (((i = n / 8) != 0 && linsert(i, '\t') == FALSE) ||
	    ((i = n % 8) != 0 && linsert(i, ' ') == FALSE)))
		return (FALSE);

	forwchar(FFRAND, soff);

	return (TRUE);
}


/*
 * Delete forward.  This is real easy, because the basic delete routine does
 * all of the work.  Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument.  Normally bound to "C-D".
 */
/* ARGSUSED */
int
forwdel(int f, int n)
{
	if (n < 0)
		return (backdel(f | FFRAND, -n));

	/* really a kill */
	if (f & FFARG) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}

	return (ldelete((RSIZE) n, (f & FFARG) ? KFORW : KNONE));
}

/*
 * Delete backwards.  This is quite easy too, because it's all done with
 * other functions.  Just move the cursor back, and delete forwards.  Like
 * delete forward, this actually does a kill if presented with an argument.
 */
/* ARGSUSED */
int
backdel(int f, int n)
{
	int	s;

	if (n < 0)
		return (forwdel(f | FFRAND, -n));

	/* really a kill */
	if (f & FFARG) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	if ((s = backchar(f | FFRAND, n)) == TRUE)
		s = ldelete((RSIZE)n, (f & FFARG) ? KFORW : KNONE);

	return (s);
}

#ifdef	NOTAB
/* ARGSUSED */
int
space_to_tabstop(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);
	return (linsert((n << 3) - (curwp->w_doto & 7), ' '));
}
#endif /* NOTAB */

/*
 * Move the dot to the first non-whitespace character of the current line.
 */
int
backtoindent(int f, int n)
{
	gotobol(FFRAND, 1);
	while (curwp->w_doto < llength(curwp->w_dotp) &&
	    (isspace(lgetc(curwp->w_dotp, curwp->w_doto))))
		++curwp->w_doto;
	return (TRUE);
}
