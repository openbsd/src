/*
 * Code for dealing with paragraphs and filling. Adapted from MicroEMACS 3.6
 * and GNU-ified by mwm@ucbvax.	 Several bug fixes by blarson@usc-oberon.
 */
#include "def.h"

static int	fillcol = 70 ;
#define MAXWORD 256

/*
 * go back to the begining of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph
 */
/*ARGSUSED*/
gotobop(f, n)
{
	if (n < 0)	/* the other way...*/
		return gotoeop(f, -n);

	while (n-- > 0) {	/* for each one asked for */

		/* first scan back until we are in a word */
		
		while (backchar(FFRAND, 1) && !inword()) {}
		curwp->w_doto = 0;	/* and go to the B-O-Line */

		/* and scan back until we hit a <NL><SP> <NL><TAB> or <NL><NL> */
		while (lback(curwp->w_dotp) != curbp->b_linep)
			if (llength(lback(curwp->w_dotp))
			    && lgetc(curwp->w_dotp,0) != ' '
			    && lgetc(curwp->w_dotp,0) != '.'
			    && lgetc(curwp->w_dotp,0) != '\t')
				curwp->w_dotp = lback(curwp->w_dotp);
			else {
		          if (llength(lback(curwp->w_dotp))
		  	      && lgetc(curwp->w_dotp,0) == '.') {
			    curwp->w_dotp = lforw(curwp->w_dotp);
			    if(curwp->w_dotp == curbp->b_linep) {
			      /* beond end of buffer, cleanup time */
		 	      curwp->w_dotp = lback(curwp->w_dotp);
			      curwp->w_doto = llength(curwp->w_dotp);
			    }
		          }
			  break;
		        }
	}
	curwp->w_flag |= WFMOVE;	/* force screen update */
	return TRUE;
}

/*
 * go forword to the end of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph
 */
/*ARGSUSED*/
gotoeop(f, n)
{
	if (n < 0)	/* the other way...*/
		return gotobop(f, -n);

	while (n-- > 0) {	/* for each one asked for */

		/* Find the first word on/after the current line */
		curwp->w_doto = 0;
		while(forwchar(FFRAND, 1) && !inword()) {}
		curwp->w_doto = 0;
		curwp->w_dotp = lforw(curwp->w_dotp);
		/* and scan forword until we hit a <NL><SP> or ... */
		while (curwp->w_dotp != curbp->b_linep) {
			if (llength(curwp->w_dotp)
			    && lgetc(curwp->w_dotp,0) != ' '
			    && lgetc(curwp->w_dotp,0) != '.'
			    && lgetc(curwp->w_dotp,0) != '\t')
				curwp->w_dotp = lforw(curwp->w_dotp);
			else
				break;
		}
		if(curwp->w_dotp == curbp->b_linep) {
			/* beond end of buffer, cleanup time */
			curwp->w_dotp = lback(curwp->w_dotp);
			curwp->w_doto = llength(curwp->w_dotp);
			break;			
		}
	}
	curwp->w_flag |= WFMOVE;	/* force screen update */
	return TRUE;
}

/*
 * Fill the current paragraph according to the current
 * fill column
 */
/*ARGSUSED*/
fillpara(f, n)
{
	register int	c;		/* current char durring scan	*/
	register int	wordlen;	/* length of current word	*/
	register int	clength;	/* position on line during fill */
	register int	i;		/* index during word copy	*/
	register int	eopflag;	/* Are we at the End-Of-Paragraph? */
	int		firstflag;	/* first word? (needs no space) */
	int		newlength;	/* tentative new line length	*/
	int		eolflag;	/* was at end of line		*/
	LINE		*eopline;	/* pointer to line just past EOP */
	char wbuf[MAXWORD];		/* buffer for current word	*/

	/* record the pointer to the line just past the EOP */
	(VOID) gotoeop(FFRAND, 1);
	if(curwp->w_doto != 0)	{
		/* paragraph ends at end of buffer */
		(VOID) lnewline();
		eopline = lforw(curwp->w_dotp);
	} else	eopline = curwp->w_dotp;

	/* and back top the begining of the paragraph */
	(VOID) gotobop(FFRAND, 1);

	/* initialize various info */
	while (!inword() && forwchar(FFRAND, 1)) {}
	clength = curwp->w_doto;
	wordlen = 0;

	/* scan through lines, filling words */
	firstflag = TRUE;
	eopflag = FALSE;
	while (!eopflag) {
		/* get the next character in the paragraph */
		if (eolflag=(curwp->w_doto == llength(curwp->w_dotp))) {
			c = ' ';
			if (lforw(curwp->w_dotp) == eopline)
				eopflag = TRUE;
		} else
			c = lgetc(curwp->w_dotp, curwp->w_doto);

		/* and then delete it */
		if (ldelete((RSIZE) 1, KNONE) == FALSE && !eopflag)
			return FALSE;

		/* if not a separator, just add it in */
		if (c != ' ' && c != '\t') {
			if (wordlen < MAXWORD - 1)
				wbuf[wordlen++] = c;
			else {
				/* You loose chars beyond MAXWORD if the word
				 * is to long. I'm to lazy to fix it now; it
				 * just silently truncated the word before, so
				 * I get to feel smug.
				 */
				ewprintf("Word too long!");
			}
		} else if (wordlen) {
			/* calculate tenatitive new length with word added */
			newlength = clength + 1 + wordlen;
			/* if at end of line or at doublespace and previous
			 * character was one of '.','?','!' doublespace here.
			 */
			if((eolflag || curwp->w_doto==llength(curwp->w_dotp)
			    || (c=lgetc(curwp->w_dotp,curwp->w_doto))==' '
			    || c=='\t')
			  && ISEOSP(wbuf[wordlen-1])
			  && wordlen<MAXWORD-1)
				wbuf[wordlen++] = ' ';
			/* at a word break with a word waiting */
			if (newlength <= fillcol) {
				/* add word to current line */
				if (!firstflag) {
					(VOID) linsert(1, ' ');
					++clength;
				}
				firstflag = FALSE;
			} else {
				if(curwp->w_doto > 0 &&
				    lgetc(curwp->w_dotp,curwp->w_doto-1)==' ') {
					curwp->w_doto -= 1;
					(VOID) ldelete((RSIZE) 1, KNONE);
				}
				/* start a new line */
				(VOID) lnewline();
				clength = 0;
			}

			/* and add the word in in either case */
			for (i=0; i<wordlen; i++) {
				(VOID) linsert(1, wbuf[i]);
				++clength;
			}
			wordlen = 0;
		}
	}
	/* and add a last newline for the end of our new paragraph */
	(VOID) lnewline();
	/* we realy should wind up where we started, (which is hard to keep
	 * track of) but I think the end of the last line is better than the
	 * begining of the blank line.	 */
	(VOID) backchar(FFRAND, 1);
	return TRUE;
}

/* delete n paragraphs starting with the current one */
/*ARGSUSED*/
killpara(f, n)
{
	register int status;	/* returned status of functions */

	while (n--) {		/* for each paragraph to delete */

		/* mark out the end and begining of the para to delete */
		(VOID) gotoeop(FFRAND, 1);

		/* set the mark here */
		curwp->w_markp = curwp->w_dotp;
		curwp->w_marko = curwp->w_doto;

		/* go to the begining of the paragraph */
		(VOID) gotobop(FFRAND, 1);
		curwp->w_doto = 0;	/* force us to the begining of line */

		/* and delete it */
		if ((status = killregion(FFRAND, 1)) != TRUE)
			return status;

		/* and clean up the 2 extra lines */
		(VOID) ldelete((RSIZE) 1, KFORW);
	}
	return TRUE;
}

/*
 * check to see if we're past fillcol, and if so,
 * justify this line. As a last step, justify the line.
 */
/*ARGSUSED*/
fillword(f, n)
{
	register char	c;
	register int	col, i, nce;

	for (i = col = 0; col <= fillcol; ++i, ++col) {
		if (i == curwp->w_doto) return selfinsert(f, n) ;
		c = lgetc(curwp->w_dotp, i);
		if (c == '\t'
#ifdef	NOTAB
			&& !(curbp->b_flag & BFNOTAB)
#endif
			) col |= 0x07;
		else if (ISCTRL(c) != FALSE) ++col;
	}
	if (curwp->w_doto != llength(curwp->w_dotp)) {
		(VOID) selfinsert(f, n);
		nce = llength(curwp->w_dotp) - curwp->w_doto;
	} else nce = 0;
	curwp->w_doto = i;

	if ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ' && c != '\t')
		do {
			(VOID) backchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' '
		      && c != '\t' && curwp->w_doto > 0);

	if (curwp->w_doto == 0)
		do {
			(VOID) forwchar(FFRAND, 1);
		} while ((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' '
		      && c != '\t' && curwp->w_doto < llength(curwp->w_dotp));

	(VOID) delwhite(FFRAND, 1);
	(VOID) lnewline();
	i = llength(curwp->w_dotp) - nce;
	curwp->w_doto = i>0 ? i : 0;
	curwp->w_flag |= WFMOVE;
	if (nce == 0 && curwp->w_doto != 0) return fillword(f, n);
	return TRUE;
}

/* Set fill column to n. */
setfillcol(f, n) {
	extern int	getcolpos() ;

	fillcol = ((f & FFARG) ? n : getcolpos());
	ewprintf("Fill column set to %d", fillcol);
	return TRUE;
}
