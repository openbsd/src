/*	$OpenBSD: region.c,v 1.11 2002/08/22 23:28:19 deraadt Exp $	*/

/*
 *		Region based commands.
 * The routines in this file deal with the region, that magic space between
 * "." and mark.  Some functions are commands.  Some functions are just for
 * internal use.
 */

#include "def.h"

static	int	getregion(REGION *);
static	int	setsize(REGION *, RSIZE);

/*
 * Kill the region.  Ask "getregion" to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 */
/* ARGSUSED */
int
killregion(f, n)
	int f, n;
{
	int	s;
	REGION	region;

	if ((s = getregion(&region)) != TRUE)
		return (s);
	/* This is a kill-type command, so do magic kill buffer stuff. */
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;
	return (ldelete(region.r_size, KFORW));
}

/*
 * Copy all of the characters in the region to the kill buffer.  Don't move
 * dot at all.  This is a bit like a kill region followed by a yank.
 */
/* ARGSUSED */
int
copyregion(f, n)
	int f, n;
{
	LINE	*linep;
	REGION	 region;
	int	 loffs;
	int	 s;

	if ((s = getregion(&region)) != TRUE)
		return s;

	/* kill type command */
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;

	/* current line */
	linep = region.r_linep;

	/* current offset */
	loffs = region.r_offset;

	while (region.r_size--) {
		if (loffs == llength(linep)) {	/* End of line.		 */
			if ((s = kinsert('\n', KFORW)) != TRUE)
				return (s);
			linep = lforw(linep);
			loffs = 0;
		} else {			/* Middle of line.	 */
			if ((s = kinsert(lgetc(linep, loffs), KFORW)) != TRUE)
				return s;
			++loffs;
		}
	}
	return TRUE;
}

/*
 * Lower case region.  Zap all of the upper case characters in the region to
 * lower case. Use the region code to set the limits. Scan the buffer, doing
 * the changes. Call "lchange" to ensure that redisplay is done in all
 * buffers.
 */
/* ARGSUSED */
int
lowerregion(f, n)
	int f, n;
{
	LINE	*linep;
	REGION	 region;
	int	 loffs, c, s;

	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if ((s = getregion(&region)) != TRUE)
		return s;

	undo_add_change(region.r_linep, region.r_offset, region.r_size);

	lchange(WFHARD);
	linep = region.r_linep;
	loffs = region.r_offset;
	while (region.r_size--) {
		if (loffs == llength(linep)) {
			linep = lforw(linep);
			loffs = 0;
		} else {
			c = lgetc(linep, loffs);
			if (ISUPPER(c) != FALSE)
				lputc(linep, loffs, TOLOWER(c));
			++loffs;
		}
	}
	return TRUE;
}

/*
 * Upper case region.  Zap all of the lower case characters in the region to
 * upper case.  Use the region code to set the limits.  Scan the buffer,
 * doing the changes.  Call "lchange" to ensure that redisplay is done in all
 * buffers.
 */
/* ARGSUSED */
int
upperregion(f, n)
	int f, n;
{
	LINE	 *linep;
	REGION	  region;
	int	  loffs, c, s;

	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if ((s = getregion(&region)) != TRUE)
		return s;

	undo_add_change(region.r_linep, region.r_offset, region.r_size);

	lchange(WFHARD);
	linep = region.r_linep;
	loffs = region.r_offset;
	while (region.r_size--) {
		if (loffs == llength(linep)) {
			linep = lforw(linep);
			loffs = 0;
		} else {
			c = lgetc(linep, loffs);
			if (ISLOWER(c) != FALSE)
				lputc(linep, loffs, TOUPPER(c));
			++loffs;
		}
	}
	return TRUE;
}

/*
 * This routine figures out the bound of the region in the current window,
 * and stores the results into the fields of the REGION structure. Dot and
 * mark are usually close together, but I don't know the order, so I scan
 * outward from dot, in both directions, looking for mark. The size is kept
 * in a long. At the end, after the size is figured out, it is assigned to
 * the size field of the region structure. If this assignment loses any bits,
 * then we print an error. This is "type independent" overflow checking. All
 * of the callers of this routine should be ready to get an ABORT status,
 * because I might add a "if regions is big, ask before clobberring" flag.
 */
static int
getregion(rp)
	REGION *rp;
{
	LINE	*flp, *blp;
	long	 fsize, bsize;

	if (curwp->w_markp == NULL) {
		ewprintf("No mark set in this window");
		return (FALSE);
	}

	/* "r_size" always ok */
	if (curwp->w_dotp == curwp->w_markp) {
		rp->r_linep = curwp->w_dotp;
		if (curwp->w_doto < curwp->w_marko) {
			rp->r_offset = curwp->w_doto;
			rp->r_size = (RSIZE)(curwp->w_marko - curwp->w_doto);
		} else {
			rp->r_offset = curwp->w_marko;
			rp->r_size = (RSIZE)(curwp->w_doto - curwp->w_marko);
		}
		return TRUE;
	}
	/* get region size */
	flp = blp = curwp->w_dotp;
	bsize = curwp->w_doto;
	fsize = llength(flp) - curwp->w_doto + 1;
	while (lforw(flp) != curbp->b_linep || lback(blp) != curbp->b_linep) {
		if (lforw(flp) != curbp->b_linep) {
			flp = lforw(flp);
			if (flp == curwp->w_markp) {
				rp->r_linep = curwp->w_dotp;
				rp->r_offset = curwp->w_doto;
				return (setsize(rp,
				    (RSIZE)(fsize + curwp->w_marko)));
			}
			fsize += llength(flp) + 1;
		}
		if (lback(blp) != curbp->b_linep) {
			blp = lback(blp);
			bsize += llength(blp) + 1;
			if (blp == curwp->w_markp) {
				rp->r_linep = blp;
				rp->r_offset = curwp->w_marko;
				return (setsize(rp,
				    (RSIZE)(bsize - curwp->w_marko)));
			}
		}
	}
	ewprintf("Bug: lost mark");
	return FALSE;
}

/*
 * Set size, and check for overflow.
 */
static int
setsize(rp, size)
	REGION *rp;
	RSIZE   size;
{
	rp->r_size = size;
	if (rp->r_size != size) {
		ewprintf("Region is too large");
		return FALSE;
	}
	return TRUE;
}

#ifdef PREFIXREGION
/*
 * Implements one of my favorite keyboard macros; put a string at the
 * beginning of a number of lines in a buffer.	The quote string is
 * settable by using set-prefix-string.	 Great for quoting mail, which
 * is the real reason I wrote it, but also has uses for creating bar
 * comments (like the one you're reading) in C code.
 */

#define PREFIXLENGTH 40
static char	prefix_string[PREFIXLENGTH] = {'>', '\0'};

/*
 * Prefix the region with whatever is in prefix_string.  Leaves dot at the
 * beginning of the line after the end of the region.  If an argument is
 * given, prompts for the line prefix string.
 */
/* ARGSUSED */
int
prefixregion(f, n)
	int f, n;
{
	LINE	*first, *last;
	REGION	 region;
	char	*prefix = prefix_string;
	int	 nline;
	int	 s;

	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if ((f == TRUE) && ((s = setprefix(FFRAND, 1)) != TRUE))
		return s;

	/* get # of lines to affect */
	if ((s = getregion(&region)) != TRUE)
		return (s);
	first = region.r_linep;
	last = (first == curwp->w_dotp) ? curwp->w_markp : curwp->w_dotp;
	for (nline = 1; first != last; nline++)
		first = lforw(first);

	/* move to beginning of region */
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;

	/* for each line, go to beginning and insert the prefix string */
	while (nline--) {
		(void)gotobol(FFRAND, 1);
		for (prefix = prefix_string; *prefix; prefix++)
			(void)linsert(1, *prefix);
		(void)forwline(FFRAND, 1);
	}
	(void)gotobol(FFRAND, 1);
	return TRUE;
}

/*
 * Set line prefix string.
 */
/* ARGSUSED */
int
setprefix(f, n)
	int f, n;
{
	char	buf[PREFIXLENGTH];
	int	s;

	if (prefix_string[0] == '\0')
		s = ereply("Prefix string: ", buf, sizeof buf);
	else
		s = ereply("Prefix string (default %s): ",
			   buf, sizeof buf, prefix_string);
	if (s == TRUE)
		(void)strlcpy(prefix_string, buf, sizeof prefix_string);
	/* CR -- use old one */
	if ((s == FALSE) && (prefix_string[0] != '\0'))
		s = TRUE;
	return s;
}
#endif /* PREFIXREGION */


int
region_get_data(REGION *reg, char *buf, int len)
{
	int i, off;
	LINE *lp;

	i = 0;
	off = reg->r_offset;
	lp = reg->r_linep;
	while (i < len) {
		if (off == llength(lp)) {
			lp = lforw(lp);
			if (lp == curwp->w_linep)
				break;
			off = 0;
			buf[i] = '\n';
		} else {
			buf[i] = lgetc(lp, off);
			off++;
		}
		i++;
	}
	return i;
}

int
region_put_data(const char *buf, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n')
			lnewline();
		else
			linsert(1, buf[i]);
	}
	return 0;
}


