/*	$OpenBSD: linefunc.c,v 1.1.1.1 1996/09/07 21:40:26 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * linefunc.c: some functions to move to the next/previous line and
 *			   to the next/previous character
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

/*
 * coladvance(col)
 *
 * Try to advance the Cursor to the specified column.
 *
 * return OK if desired column is reached, FAIL if not
 */

	int
coladvance(wcol)
	colnr_t 		wcol;
{
	int 				idx;
	register char_u		*ptr;
	register colnr_t	col;

	ptr = ml_get_curline();

	/* try to advance to the specified column */
	idx = -1;
	col = 0;
	while (col <= wcol && *ptr)
	{
		++idx;
		/* Count a tab for what it's worth (if list mode not on) */
		col += lbr_chartabsize(ptr, col);
		++ptr;
	}
	/*
	 * in insert mode it is allowed to be one char beyond the end of the line
	 */
	if ((State & INSERT) && col <= wcol)
		++idx;
	if (idx < 0)
		curwin->w_cursor.col = 0;
	else
		curwin->w_cursor.col = idx;
	if (col <= wcol)
		return FAIL;		/* Couldn't reach column */
	else
		return OK;			/* Reached column */
}

/*
 * inc(p)
 *
 * Increment the line pointer 'p' crossing line boundaries as necessary.
 * Return 1 when crossing a line, -1 when at end of file, 0 otherwise.
 */
	int
inc_cursor()
{
	return inc(&curwin->w_cursor);
}

	int
inc(lp)
	register FPOS  *lp;
{
	register char_u  *p = ml_get_pos(lp);

	if (*p != NUL)		/* still within line, move to next char (may be NUL) */
	{
		lp->col++;
		return ((p[1] != NUL) ? 0 : 1);
	}
	if (lp->lnum != curbuf->b_ml.ml_line_count)		/* there is a next line */
	{
		lp->col = 0;
		lp->lnum++;
		return 1;
	}
	return -1;
}

/*
 * incl(lp): same as inc(), but skip the NUL at the end of non-empty lines
 */
	int
incl(lp)
	register FPOS *lp;
{
	register int r;

	if ((r = inc(lp)) == 1 && lp->col)
		r = inc(lp);
	return r;
}

/*
 * dec(p)
 *
 * Decrement the line pointer 'p' crossing line boundaries as necessary.
 * Return 1 when crossing a line, -1 when at start of file, 0 otherwise.
 */
	int
dec_cursor()
{
	return dec(&curwin->w_cursor);
}

	int
dec(lp)
	register FPOS  *lp;
{
	if (lp->col > 0)
	{			/* still within line */
		lp->col--;
		return 0;
	}
	if (lp->lnum > 1)
	{			/* there is a prior line */
		lp->lnum--;
		lp->col = STRLEN(ml_get(lp->lnum));
		return 1;
	}
	return -1;					/* at start of file */
}

/*
 * decl(lp): same as dec(), but skip the NUL at the end of non-empty lines
 */
	int
decl(lp)
	register FPOS *lp;
{
	register int r;

	if ((r = dec(lp)) == 1 && lp->col)
		r = dec(lp);
	return r;
}

/*
 * make sure curwin->w_cursor in on a valid character
 */
	void
adjust_cursor()
{
	colnr_t len;

	if (curwin->w_cursor.lnum == 0)
		curwin->w_cursor.lnum = 1;
	if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;

	len = STRLEN(ml_get_curline());
	if (len == 0)
		curwin->w_cursor.col = 0;
	else if (curwin->w_cursor.col >= len)
		curwin->w_cursor.col = len - 1;
}
