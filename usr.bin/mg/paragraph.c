/*	$OpenBSD: paragraph.c,v 1.34 2014/10/17 13:25:13 lum Exp $	*/

/* This file is in the public domain. */

/*
 * Code for dealing with paragraphs and filling. Adapted from MicroEMACS 3.6
 * and GNU-ified by mwm@ucbvax.	 Several bug fixes by blarson@usc-oberon.
 */

#include <ctype.h>

#include "def.h"

static int	fillcol = 70;

#define MAXWORD 256

/*
 * Move to start of paragraph.
 * Move backwards by line, checking from the 1st character forwards for the
 * existence a non-space. If a non-space character is found, move to the 
 * preceding line. Keep doing this until a line with only spaces is found or
 * the start of buffer.
 */
/* ARGSUSED */
int
gotobop(int f, int n)
{
	int col, nospace;

	/* the other way... */
	if (n < 0)
		return (gotoeop(f, -n));

	while (n-- > 0) {
		nospace = 0;
		while (lback(curwp->w_dotp) != curbp->b_headp) {
			curwp->w_doto = 0;
			col = 0;

			while (col < llength(curwp->w_dotp) &&
			    (isspace(lgetc(curwp->w_dotp, col))))
				col++;

			if (col >= llength(curwp->w_dotp)) {
				if (nospace)
					break;
			} else
				nospace = 1;

			curwp->w_dotline--;
			curwp->w_dotp = lback(curwp->w_dotp);
		}
	}
	/* force screen update */
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Move to end of paragraph.
 * See comments for gotobop(). Same, but moving forwards.
 */
/* ARGSUSED */
int
gotoeop(int f, int n)
{
	int col, nospace;

	/* the other way... */
	if (n < 0)
		return (gotobop(f, -n));

	/* for each one asked for */
	while (n-- > 0) {
		nospace = 0;
		while (lforw(curwp->w_dotp) != curbp->b_headp) {
			col = 0;
			curwp->w_doto = 0;

			while (col < llength(curwp->w_dotp) &&
			    (isspace(lgetc(curwp->w_dotp, col))))
				col++;

			if (col >= llength(curwp->w_dotp)) {
				if (nospace)
					break;
			} else
				nospace = 1;

			curwp->w_dotp = lforw(curwp->w_dotp);
			curwp->w_dotline++;

			/* do not continue after end of buffer */
			if (lforw(curwp->w_dotp) == curbp->b_headp) {
				gotoeol(FFRAND, 1);
				curwp->w_rflag |= WFMOVE;
				return (FALSE);
			}
		}
	}

	/* force screen update */
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Justify a paragraph.  Fill the current paragraph according to the current
 * fill column.
 */
/* ARGSUSED */
int
fillpara(int f, int n)
{
	int	 c;		/* current char during scan		*/
	int	 wordlen;	/* length of current word		*/
	int	 clength;	/* position on line during fill		*/
	int	 i;		/* index during word copy		*/
	int	 eopflag;	/* Are we at the End-Of-Paragraph?	*/
	int	 firstflag;	/* first word? (needs no space)		*/
	int	 newlength;	/* tentative new line length		*/
	int	 eolflag;	/* was at end of line			*/
	int	 retval;	/* return value				*/
	struct line	*eopline;	/* pointer to line just past EOP	*/
	char	 wbuf[MAXWORD];	/* buffer for current word		*/

	undo_boundary_enable(FFRAND, 0);

	/* record the pointer to the line just past the EOP */
	(void)gotoeop(FFRAND, 1);
	if (curwp->w_doto != 0) {
		/* paragraph ends at end of buffer */
		(void)lnewline();
		eopline = lforw(curwp->w_dotp);
	} else
		eopline = curwp->w_dotp;

	/* and back top the begining of the paragraph */
	(void)gotobop(FFRAND, 1);

	/* initialize various info */
	while (inword() == 0 && forwchar(FFRAND, 1));

	clength = curwp->w_doto;
	wordlen = 0;

	/* scan through lines, filling words */
	firstflag = TRUE;
	eopflag = FALSE;
	while (!eopflag) {

		/* get the next character in the paragraph */
		if ((eolflag = (curwp->w_doto == llength(curwp->w_dotp)))) {
			c = ' ';
			if (lforw(curwp->w_dotp) == eopline)
				eopflag = TRUE;
		} else
			c = lgetc(curwp->w_dotp, curwp->w_doto);

		/* and then delete it */
		if (ldelete((RSIZE) 1, KNONE) == FALSE && !eopflag) {
			retval = FALSE;
			goto cleanup;
		}

		/* if not a separator, just add it in */
		if (c != ' ' && c != '\t') {
			if (wordlen < MAXWORD - 1)
				wbuf[wordlen++] = c;
			else {
				/*
				 * You lose chars beyond MAXWORD if the word
				 * is too long. I'm too lazy to fix it now; it
				 * just silently truncated the word before,
				 * so I get to feel smug.
				 */
				ewprintf("Word too long!");
			}
		} else if (wordlen) {

			/* calculate tentative new length with word added */
			newlength = clength + 1 + wordlen;

			/*
			 * if at end of line or at doublespace and previous
			 * character was one of '.','?','!' doublespace here.
			 * behave the same way if a ')' is preceded by a
			 * [.?!] and followed by a doublespace.
			 */
			if ((eolflag ||
			    curwp->w_doto == llength(curwp->w_dotp) ||
			    (c = lgetc(curwp->w_dotp, curwp->w_doto)) == ' '
			    || c == '\t') && (ISEOSP(wbuf[wordlen - 1]) ||
			    (wbuf[wordlen - 1] == ')' && wordlen >= 2 &&
			    ISEOSP(wbuf[wordlen - 2]))) &&
			    wordlen < MAXWORD - 1)
				wbuf[wordlen++] = ' ';

			/* at a word break with a word waiting */
			if (newlength <= fillcol) {
				/* add word to current line */
				if (!firstflag) {
					(void)linsert(1, ' ');
					++clength;
				}
				firstflag = FALSE;
			} else {
				if (curwp->w_doto > 0 &&
				    lgetc(curwp->w_dotp, curwp->w_doto - 1) == ' ') {
					curwp->w_doto -= 1;
					(void)ldelete((RSIZE) 1, KNONE);
				}
				/* start a new line */
				(void)lnewline();
				clength = 0;
			}

			/* and add the word in in either case */
			for (i = 0; i < wordlen; i++) {
				(void)linsert(1, wbuf[i]);
				++clength;
			}
			wordlen = 0;
		}
	}
	/* and add a last newline for the end of our new paragraph */
	(void)lnewline();

	/*
	 * We really should wind up where we started, (which is hard to keep
	 * track of) but I think the end of the last line is better than the
	 * beginning of the blank line.
	 */
	(void)backchar(FFRAND, 1);
	retval = TRUE;
cleanup:
	undo_boundary_enable(FFRAND, 1);
	return (retval);
}

/*
 * Delete a paragraph.  Delete n paragraphs starting with the current one.
 */
/* ARGSUSED */
int
killpara(int f, int n)
{
	int	status, end = FALSE;	/* returned status of functions */

	/* for each paragraph to delete */
	while (n--) {

		/* mark out the end and beginning of the para to delete */
		if (!gotoeop(FFRAND, 1))
			end = TRUE;

		/* set the mark here */
		curwp->w_markp = curwp->w_dotp;
		curwp->w_marko = curwp->w_doto;

		/* go to the beginning of the paragraph */
		(void)gotobop(FFRAND, 1);

		/* force us to the beginning of line */
		curwp->w_doto = 0;

		/* and delete it */
		if ((status = killregion(FFRAND, 1)) != TRUE)
			return (status);

		if (end)
			return (TRUE);
	}
	return (TRUE);
}

/*
 * Insert char with work wrap.  Check to see if we're past fillcol, and if so,
 * justify this line.  As a last step, justify the line.
 */
/* ARGSUSED */
int
fillword(int f, int n)
{
	char	c;
	int	col, i, nce;

	for (i = col = 0; col <= fillcol; ++i, ++col) {
		if (i == curwp->w_doto)
			return selfinsert(f, n);
		c = lgetc(curwp->w_dotp, i);
		if (c == '\t'
#ifdef NOTAB
		    && !(curbp->b_flag & BFNOTAB)
#endif
			)
			col |= 0x07;
		else if (ISCTRL(c) != FALSE)
			++col;
	}
	if (curwp->w_doto != llength(curwp->w_dotp)) {
		(void)selfinsert(f, n);
		nce = llength(curwp->w_dotp) - curwp->w_doto;
	} else
		nce = 0;
	curwp->w_doto = i;

	if ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' && c != '\t')
		do {
			(void)backchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' &&
		    c != '\t' && curwp->w_doto > 0);

	if (curwp->w_doto == 0)
		do {
			(void)forwchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' &&
		    c != '\t' && curwp->w_doto < llength(curwp->w_dotp));

	(void)delwhite(FFRAND, 1);
	(void)lnewline();
	i = llength(curwp->w_dotp) - nce;
	curwp->w_doto = i > 0 ? i : 0;
	curwp->w_rflag |= WFMOVE;
	if (nce == 0 && curwp->w_doto != 0)
		return (fillword(f, n));
	return (TRUE);
}

/*
 * Set fill column to n for justify.
 */
int
setfillcol(int f, int n)
{
	char buf[32], *rep;
	const char *es;
	int nfill;

	if ((f & FFARG) != 0) {
		fillcol = n;
	} else {
		if ((rep = eread("Set fill-column: ", buf, sizeof(buf),
		    EFNEW | EFCR)) == NULL)
			return (ABORT);
		else if (rep[0] == '\0')
			return (FALSE);
		nfill = strtonum(rep, 0, INT_MAX, &es);
		if (es != NULL) {
			dobeep();
			ewprintf("Invalid fill column: %s", rep);
			return (FALSE);
		}
		fillcol = nfill;
		ewprintf("Fill column set to %d", fillcol);
	}
	return (TRUE);
}
