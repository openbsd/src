/*
 *		Region based commands.
 * The routines in this file
 * deal with the region, that magic space
 * between "." and mark. Some functions are
 * commands. Some functions are just for
 * internal use.
 */
#include	"def.h"

/*
 * Kill the region. Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 */
/*ARGSUSED*/
killregion(f, n)
{
	register int	s;
	REGION		region;

	if ((s=getregion(&region)) != TRUE)
		return (s);
	if ((lastflag&CFKILL) == 0)		/* This is a kill type	*/
		kdelete();			/* command, so do magic */
	thisflag |= CFKILL;			/* kill buffer stuff.	*/
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;
	return (ldelete(region.r_size, KFORW));
}

/*
 * Copy all of the characters in the
 * region to the kill buffer. Don't move dot
 * at all. This is a bit like a kill region followed
 * by a yank.
 */
/*ARGSUSED*/
copyregion(f, n)
{
	register LINE	*linep;
	register int	loffs;
	register int	s;
	REGION		region;
	VOID		kdelete();

	if ((s=getregion(&region)) != TRUE)
		return s;
	if ((lastflag&CFKILL) == 0)		/* Kill type command.	*/
		kdelete();
	thisflag |= CFKILL;
	linep = region.r_linep;			/* Current line.	*/
	loffs = region.r_offset;		/* Current offset.	*/
	while (region.r_size--) {
		if (loffs == llength(linep)) {	/* End of line.		*/
			if ((s=kinsert('\n', KFORW)) != TRUE)
				return (s);
			linep = lforw(linep);
			loffs = 0;
		} else {			/* Middle of line.	*/
			if ((s=kinsert(lgetc(linep, loffs), KFORW)) != TRUE)
				return s;
			++loffs;
		}
	}
	return TRUE;
}

/*
 * Lower case region. Zap all of the upper
 * case characters in the region to lower case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers.
 */
/*ARGSUSED*/
lowerregion(f, n)
{
	register LINE	*linep;
	register int	loffs;
	register int	c;
	register int	s;
	REGION		region;

	if ((s=getregion(&region)) != TRUE)
		return s;
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
 * Upper case region. Zap all of the lower
 * case characters in the region to upper case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers.
 */
/*ARGSUSED*/
upperregion(f, n)
{
	register LINE	*linep;
	register int	loffs;
	register int	c;
	register int	s;
	REGION		region;
	VOID		lchange();

	if ((s=getregion(&region)) != TRUE)
		return s;
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
 * This routine figures out the bound of the region
 * in the current window, and stores the results into the fields
 * of the REGION structure. Dot and mark are usually close together,
 * but I don't know the order, so I scan outward from dot, in both
 * directions, looking for mark. The size is kept in a long. At the
 * end, after the size is figured out, it is assigned to the size
 * field of the region structure. If this assignment loses any bits,
 * then we print an error. This is "type independent" overflow
 * checking. All of the callers of this routine should be ready to
 * get an ABORT status, because I might add a "if regions is big,
 * ask before clobberring" flag.
 */
getregion(rp) register REGION *rp; {
	register LINE	*flp;
	register LINE	*blp;
	register long	fsize;			/* Long now.		*/
	register long	bsize;

	if (curwp->w_markp == NULL) {
		ewprintf("No mark set in this window");
		return (FALSE);
	}
	if (curwp->w_dotp == curwp->w_markp) {	/* "r_size" always ok.	*/
		rp->r_linep = curwp->w_dotp;
		if (curwp->w_doto < curwp->w_marko) {
			rp->r_offset = curwp->w_doto;
			rp->r_size = (RSIZE) (curwp->w_marko-curwp->w_doto);
		} else {
			rp->r_offset = curwp->w_marko;
			rp->r_size = (RSIZE) (curwp->w_doto-curwp->w_marko);
		}
		return TRUE;
	}
	flp = blp = curwp->w_dotp;		/* Get region size.	*/
	bsize = curwp->w_doto;
	fsize = llength(flp)-curwp->w_doto+1;
	while (lforw(flp)!=curbp->b_linep || lback(blp)!=curbp->b_linep) {
		if (lforw(flp) != curbp->b_linep) {
			flp = lforw(flp);
			if (flp == curwp->w_markp) {
				rp->r_linep = curwp->w_dotp;
				rp->r_offset = curwp->w_doto;
				return (setsize(rp,
					(RSIZE) (fsize+curwp->w_marko)));
			}
			fsize += llength(flp)+1;
		}
		if (lback(blp) != curbp->b_linep) {
			blp = lback(blp);
			bsize += llength(blp)+1;
			if (blp == curwp->w_markp) {
				rp->r_linep = blp;
				rp->r_offset = curwp->w_marko;
				return (setsize(rp,
					(RSIZE) (bsize-curwp->w_marko)));
			}
		}
	}
	ewprintf("Bug: lost mark");		/* Gak!			*/
	return FALSE;
}

/*
 * Set size, and check for overflow.
 */
setsize(rp, size) register REGION *rp; register RSIZE size; {

	rp->r_size = size;
	if (rp->r_size != size) {
		ewprintf("Region is too large");
		return FALSE;
	}
	return TRUE;
}

#ifdef	PREFIXREGION
/*
 * Implements one of my favorite keyboard macros; put a string at the
 * beginning of a number of lines in a buffer.	The quote string is
 * settable by using set-prefix-string.	 Great for quoting mail, which
 * is the real reason I wrote it, but also has uses for creating bar
 * comments (like the one you're reading) in C code.
 */

#define PREFIXLENGTH 40
static char prefix_string[PREFIXLENGTH] = { '>', '\0' };

/*
 * Prefix the region with whatever is in prefix_string.
 * Leaves dot at the beginning of the line after the end
 * of the region.  If an argument is given, prompts for the
 * line prefix string.
 */

/*ARGSUSED*/
prefixregion(f, n)
{
	register int	s;
	register LINE	*first, *last;
	register int	nline;
	REGION		region;
	char		*prefix = prefix_string;

	if ((f == TRUE) && ((s = setprefix(FFRAND, 1)) != TRUE))
		return s;

	/* get # of lines to affect */
	if ((s = getregion(&region)) != TRUE)
		return (s);
	first = region.r_linep;
	last = (first == curwp->w_dotp) ? curwp->w_markp : curwp->w_dotp;
	for (nline = 1; first != last; nline++)
		first = lforw(first);

	/*move to beginning of region */
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;

	/* for each line, go to beginning and insert the prefix string */
	while (nline--) {
		(VOID) gotobol(FFRAND, 1);
		for (prefix = prefix_string; *prefix; prefix++)
			(VOID) linsert(1, *prefix);
		(VOID) forwline(FFRAND, 1);
	}
	(VOID) gotobol(FFRAND, 1);
	return TRUE;
}

/*
 * Set prefix string.
 */

/*ARGSUSED*/
setprefix(f, n)
{
	char		buf[PREFIXLENGTH];
	register int	s;

	if (prefix_string[0] == '\0')
		s = ereply("Prefix string: ",buf,sizeof buf);
	else
		s = ereply("Prefix string (default %s): ",
				buf,sizeof buf,prefix_string);
	if (s == TRUE)
		(VOID) strcpy(prefix_string, buf);
	if ((s == FALSE) && (prefix_string[0] != '\0')) /* CR -- use old one */
		s = TRUE;
	return s;
}
#endif
