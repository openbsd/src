/*
 *		Buffer handling.
 */
#include	"def.h"
#include	"kbd.h"			/* needed for modes */

static RSIZE	itor();

/*
 * Attach a buffer to a window. The values of dot and mark come
 * from the buffer if the use count is 0. Otherwise, they come
 * from some other window.  *scratch* is the default alternate
 * buffer.
 */
/*ARGSUSED*/
usebuffer(f, n)
{
	register BUFFER *bp;
	register int	s;
	char		bufn[NBUFN];

	/* Get buffer to use from user */
	if ((curbp->b_altb == NULL)
	    && ((curbp->b_altb = bfind("*scratch*", TRUE)) == NULL))
		s=eread("Switch to buffer: ", bufn, NBUFN, EFNEW|EFBUF);
	else
		s=eread("Switch to buffer: (default %s) ", bufn, NBUFN,
			 EFNEW|EFBUF, curbp->b_altb->b_bname);

	if (s == ABORT) return s;
	if (s == FALSE && curbp->b_altb != NULL) bp = curbp->b_altb ;
	else if ((bp=bfind(bufn, TRUE)) == NULL) return FALSE;

	/* and put it in current window */
	curbp = bp;
	return showbuffer(bp, curwp, WFFORCE|WFHARD);
}

/*
 * pop to buffer asked for by the user.
 */
/*ARGSUSED*/
poptobuffer(f, n)
{
	register BUFFER *bp;
	register MGWIN *wp;
	register int	s;
	char		bufn[NBUFN];
	MGWIN	*popbuf();

	/* Get buffer to use from user */
	if ((curbp->b_altb == NULL)
	    && ((curbp->b_altb = bfind("*scratch*", TRUE)) == NULL))
		s=eread("Switch to buffer in other window: ", bufn, NBUFN,
			EFNEW|EFBUF);
	else
		s=eread("Switch to buffer in other window: (default %s) ",
			 bufn, NBUFN, EFNEW|EFBUF, curbp->b_altb->b_bname);
	if (s == ABORT) return s;
	if (s == FALSE && curbp->b_altb != NULL) bp = curbp->b_altb ;
	else if ((bp=bfind(bufn, TRUE)) == NULL) return FALSE;

	/* and put it in a new window */
	if ((wp = popbuf(bp)) == NULL) return FALSE;
	curbp = bp;
	curwp = wp;
	return TRUE;
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too
 * upset if it isn't there at all!). Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
/*ARGSUSED*/
killbuffer(f, n)
{
	register BUFFER *bp;
	register BUFFER *bp1;
	register BUFFER *bp2;
	MGWIN		*wp;
	register int	s;
	char		bufn[NBUFN];

	if ((s=eread("Kill buffer: (default %s) ", bufn, NBUFN, EFNEW|EFBUF,
		    curbp->b_bname)) == ABORT) return (s);
	else if (s == FALSE) bp = curbp;
	else if ((bp=bfind(bufn, FALSE)) == NULL) return FALSE;

	/* find some other buffer to display. try the alternate buffer,
	 * then the first different buffer in the buffer list.	if
	 * there's only one buffer, create buffer *scratch* and make
	 * it the alternate buffer.  return if *scratch* is only buffer
	 */
	if ((bp1 = bp->b_altb) == NULL) {
		bp1 = (bp == bheadp) ? bp->b_bufp : bheadp;
		if (bp1 == NULL) {
			/* only one buffer. see if it's *scratch* */
			if (bp == bfind("*scratch*",FALSE))
				return FALSE;
			/* create *scratch* for alternate buffer */
			if ((bp1 = bfind("*scratch*",TRUE)) == NULL)
				return FALSE;
		}
	}
	if (bclear(bp) != TRUE) return TRUE;
	for (wp = wheadp; bp->b_nwnd > 0; wp = wp->w_wndp) {
	    if (wp->w_bufp == bp) {
		bp2 = bp1->b_altb;		/* save alternate buffer */
		if(showbuffer(bp1, wp, WFMODE|WFFORCE|WFHARD) != NULL)
			bp1->b_altb = bp2;
		else	bp1 = bp2;
	    }
	}
	if (bp == curbp) curbp = bp1;
	free((char *) bp->b_linep);		/* Release header line. */
	bp2 = NULL;				/* Find the header.	*/
	bp1 = bheadp;
	while (bp1 != bp) {
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp2 = bp1;
		bp1 = bp1->b_bufp;
	}
	bp1 = bp1->b_bufp;			/* Next one in chain.	*/
	if (bp2 == NULL)			/* Unlink it.		*/
		bheadp = bp1;
	else
		bp2->b_bufp = bp1;
	while (bp1 != NULL) {			/* Finish with altb's	*/
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp1 = bp1->b_bufp;
	}
	free(bp->b_bname);			/* Release name block	*/
	free((char *) bp);			/* Release buffer block */
	return TRUE;
}

/*
 * Save some buffers - just call anycb with the arg flag.
 */
/*ARGSUSED*/
savebuffers(f, n)
{
	if (anycb(f) == ABORT) return ABORT;
	return TRUE;
}

/*
 * Display the buffer list. This is done
 * in two parts. The "makelist" routine figures out
 * the text, and puts it in a buffer. "popbuf"
 * then pops the data onto the screen. Bound to
 * "C-X C-B".
 */
/*ARGSUSED*/
listbuffers(f, n)
{
	register BUFFER *bp;
	register MGWIN *wp;
	BUFFER		*makelist();
	MGWIN		*popbuf();

	if ((bp=makelist()) == NULL || (wp=popbuf(bp)) == NULL)
		return FALSE;
	wp->w_dotp = bp->b_dotp;	/* fix up if window already on screen */
	wp->w_doto = bp->b_doto;
	return TRUE;
}

/*
 * This routine rebuilds the text for the
 * list buffers command. Return TRUE if
 * everything works. Return FALSE if there
 * is an error (if there is no memory).
 */
BUFFER *
makelist() {
	register char	*cp1;
	register char	*cp2;
	register int	c;
	register BUFFER *bp;
	LINE		*lp;
	register RSIZE	nbytes;
	BUFFER		*blp;
	char		b[6+1];
	char		line[128];

	if ((blp = bfind("*Buffer List*", TRUE)) == NULL) return NULL;
	if (bclear(blp) != TRUE) return NULL;
	blp->b_flag &= ~BFCHG;			/* Blow away old.	*/

	(VOID) strcpy(line, " MR Buffer");
	cp1 = line + 10;
	while(cp1 < line + 4 + NBUFN + 1) *cp1++ = ' ';
	(VOID) strcpy(cp1, "Size   File");
	if (addline(blp, line) == FALSE) return NULL;
	(VOID) strcpy(line, " -- ------");
	cp1 = line + 10;
	while(cp1 < line + 4 + NBUFN + 1) *cp1++ = ' ';
	(VOID) strcpy(cp1, "----   ----");
	if (addline(blp, line) == FALSE) return NULL;
	bp = bheadp;				/* For all buffers	*/
	while (bp != NULL) {
		cp1 = &line[0];			/* Start at left edge	*/
		*cp1++ = (bp == curbp) ? '.' : ' ';
		*cp1++ = ((bp->b_flag&BFCHG) != 0) ? '*' : ' ';
		*cp1++ = ' ';			/* Gap.			*/
		*cp1++ = ' ';
		cp2 = &bp->b_bname[0];		/* Buffer name		*/
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		while (cp1 < &line[4+NBUFN+1])
			*cp1++ = ' ';
		nbytes = 0;			/* Count bytes in buf.	*/
		if (bp != blp) {
			lp = lforw(bp->b_linep);
			while (lp != bp->b_linep) {
				nbytes += llength(lp)+1;
				lp = lforw(lp);
			}
			if(nbytes) nbytes--;	/* no bonus newline	*/
		}
		(VOID) itor(b, 6, nbytes);	/* 6 digit buffer size. */
		cp2 = &b[0];
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		*cp1++ = ' ';			/* Gap..			*/
		cp2 = &bp->b_fname[0];		/* File name		*/
		if (*cp2 != 0) {
			while ((c = *cp2++) != 0) {
				if (cp1 < &line[128-1])
					*cp1++ = c;
			}
		}
		*cp1 = 0;			/* Add to the buffer.	*/
		if (addline(blp, line) == FALSE)
			return NULL;
		bp = bp->b_bufp;
	}
	blp->b_dotp = lforw(blp->b_linep);	/* put dot at beginning of buffer */
	blp->b_doto = 0;
	return blp;				/* All done		*/
}

/*
 * Used above.
 */
static RSIZE itor(buf, width, num)
register char buf[]; register int width; register RSIZE num; {
	register RSIZE r;

	if (num / 10 == 0) {
		buf[0] = (num % 10) + '0';
		for (r = 1; r < width; buf[r++] = ' ')
			;
		buf[width] = '\0';
		return 1;
	} else {
		buf[r = itor(buf, width, num / (RSIZE)10)] =
				(num % (RSIZE)10) + '0';
		return r + 1;
	}
	/*NOTREACHED*/
}

/*
 * The argument "text" points to
 * a string. Append this line to the
 * buffer. Handcraft the EOL
 * on the end. Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
addline(bp, text) register BUFFER *bp; char *text; {
	register LINE	*lp;
	register int	i;
	register int	ntext;

	ntext = strlen(text);
	if ((lp=lalloc(ntext)) == NULL)
		return FALSE;
	for (i=0; i<ntext; ++i)
		lputc(lp, i, text[i]);
	bp->b_linep->l_bp->l_fp = lp;		/* Hook onto the end	*/
	lp->l_bp = bp->b_linep->l_bp;
	bp->b_linep->l_bp = lp;
	lp->l_fp = bp->b_linep;
#ifdef CANTHAPPEN
	if (bp->b_dotp == bp->b_linep)		/* If "." is at the end	*/
		bp->b_dotp = lp;		/* move it to new line	*/
	if (bp->b_markp == bp->b_linep)		/* ditto for mark	*/
		bp->b_markp = lp;
#endif
	return TRUE;
}

/*
 * Look through the list of buffers, giving the user
 * a chance to save them.  Return TRUE if there are
 * any changed buffers afterwards. Buffers that don't
 * have an associated file don't count. Return FALSE
 * if there are no changed buffers.
 */
anycb(f) {
	register BUFFER *bp;
	register int	s = FALSE, save = FALSE;
	char		prompt[NFILEN + 11];
	VOID		upmodes();

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (*(bp->b_fname) != '\0'
		&&  (bp->b_flag&BFCHG) != 0) {
			(VOID) strcpy(prompt, "Save file ");
			(VOID) strcpy(prompt + 10, bp->b_fname);
			if ((f == TRUE || (save = eyorn(prompt)) == TRUE)
			&&  buffsave(bp) == TRUE) {
				bp->b_flag &= ~BFCHG;
				upmodes(bp);
			} else s = TRUE;
			if (save == ABORT) return (save);
			save = TRUE;
		}
	}
	if (save == FALSE /* && kbdmop == NULL */ )	/* experimental */
		ewprintf("(No files need saving)");
	return s;
}

/*
 * Search for a buffer, by name.
 * If not found, and the "cflag" is TRUE,
 * create a buffer and put it in the list of
 * all buffers. Return pointer to the BUFFER
 * block for the buffer.
 */
BUFFER	*
bfind(bname, cflag) register char *bname; {
	register BUFFER *bp;
	char		*malloc();
	register LINE	*lp;
	int i;
	extern int defb_nmodes;
	extern MAPS *defb_modes[PBMODES];
	extern int defb_flag;

	bp = bheadp;
	while (bp != NULL) {
		if (fncmp(bname, bp->b_bname) == 0)
			return bp;
		bp = bp->b_bufp;
	}
	if (cflag!=TRUE) return NULL;
	/*NOSTRICT*/
	if ((bp=(BUFFER *)malloc(sizeof(BUFFER))) == NULL) {
		ewprintf("Can't get %d bytes", sizeof(BUFFER));
		return NULL;
	}
	if ((bp->b_bname=malloc((unsigned)(strlen(bname)+1))) == NULL) {
		ewprintf("Can't get %d bytes", strlen(bname)+1);
		free((char *) bp);
		return NULL;
	}
	if ((lp = lalloc(0)) == NULL) {
		free(bp->b_bname);
		free((char *) bp);
		return NULL;
	}
	bp->b_altb = bp->b_bufp	 = NULL;
	bp->b_dotp  = lp;
	bp->b_doto  = 0;
	bp->b_markp = NULL;
	bp->b_marko = 0;
	bp->b_flag  = defb_flag;
	bp->b_nwnd  = 0;
	bp->b_linep = lp;
	bp->b_nmodes = defb_nmodes;
	i = 0;
	do {
	    bp->b_modes[i] = defb_modes[i];
	} while(i++ < defb_nmodes);
	bp->b_fname[0] = '\0';
	bzero(&bp->b_fi, sizeof(bp->b_fi));
	(VOID) strcpy(bp->b_bname, bname);
	lp->l_fp = lp;
	lp->l_bp = lp;
	bp->b_bufp = bheadp;
	bheadp = bp;
	return bp;
}

/*
 * This routine blows away all of the text
 * in a buffer. If the buffer is marked as changed
 * then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The
 * window chain is nearly always wrong if this gets
 * called; the caller must arrange for the updates
 * that are required. Return TRUE if everything
 * looks good.
 */
bclear(bp) register BUFFER *bp; {
	register LINE	*lp;
	register int	s;
	VOID		lfree();

	if ((bp->b_flag&BFCHG) != 0		/* Changed.		*/
	&& (s=eyesno("Buffer modified; kill anyway")) != TRUE)
		return (s);
	bp->b_flag  &= ~BFCHG;			/* Not changed		*/
	while ((lp=lforw(bp->b_linep)) != bp->b_linep)
		lfree(lp);
	bp->b_dotp  = bp->b_linep;		/* Fix "."		*/
	bp->b_doto  = 0;
	bp->b_markp = NULL;			/* Invalidate "mark"	*/
	bp->b_marko = 0;
	return TRUE;
}

/*
 * Display the given buffer in the given window. Flags indicated
 * action on redisplay.
 */
showbuffer(bp, wp, flags) register BUFFER *bp; register MGWIN *wp; {
	register BUFFER *obp;
	MGWIN		*owp;

	if (wp->w_bufp == bp) {			/* Easy case!	*/
		wp->w_flag |= flags;
		return TRUE;
	}

	/* First, dettach the old buffer from the window */
	if ((bp->b_altb = obp = wp->w_bufp) != NULL) {
		if (--obp->b_nwnd == 0) {
			obp->b_dotp  = wp->w_dotp;
			obp->b_doto  = wp->w_doto;
			obp->b_markp = wp->w_markp;
			obp->b_marko = wp->w_marko;
		}
	}

	/* Now, attach the new buffer to the window */
	wp->w_bufp = bp;

	if (bp->b_nwnd++ == 0) {		/* First use.		*/
		wp->w_dotp  = bp->b_dotp;
		wp->w_doto  = bp->b_doto;
		wp->w_markp = bp->b_markp;
		wp->w_marko = bp->b_marko;
	} else
	/* already on screen, steal values from other window */
		for (owp = wheadp; owp != NULL; owp = wp->w_wndp)
			if (wp->w_bufp == bp && owp != wp) {
				wp->w_dotp  = owp->w_dotp;
				wp->w_doto  = owp->w_doto;
				wp->w_markp = owp->w_markp;
				wp->w_marko = owp->w_marko;
				break;
			}
	wp->w_flag |= WFMODE|flags;
	return TRUE;
}

/*
 * Pop the buffer we got passed onto the screen.
 * Returns a status.
 */
MGWIN *
popbuf(bp) register BUFFER *bp; {
	register MGWIN *wp;

	if (bp->b_nwnd == 0) {		/* Not on screen yet.	*/
		if ((wp=wpopup()) == NULL) return NULL;
	} else
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_flag |= WFHARD|WFFORCE;
				return wp ;
			}
	if (showbuffer(bp, wp, WFHARD) != TRUE) return NULL;
	return wp;
}

/*
 * Insert another buffer at dot.  Very useful.
 */
/*ARGSUSED*/
bufferinsert(f, n)
{
	register BUFFER *bp;
	register LINE	*clp;
	register int	clo;
	register int	nline;
	int		s;
	char		bufn[NBUFN];

	/* Get buffer to use from user */
	if (curbp->b_altb != NULL)
		s=eread("Insert buffer: (default %s) ", bufn, NBUFN,
			 EFNEW|EFBUF, &(curbp->b_altb->b_bname),
			 (char *) NULL) ;
	else
		s=eread("Insert buffer: ", bufn, NBUFN, EFNEW|EFBUF,
			 (char *) NULL) ;
	if (s == ABORT) return (s);
	if (s == FALSE && curbp->b_altb != NULL) bp = curbp->b_altb;
	else if ((bp=bfind(bufn, FALSE)) == NULL) return FALSE;

	if (bp==curbp) {
		ewprintf("Cannot insert buffer into self");
		return FALSE;
	}

	/* insert the buffer */
	nline = 0;
	clp = lforw(bp->b_linep);
	for(;;) {
		for (clo = 0; clo < llength(clp); clo++)
			if (linsert(1, lgetc(clp, clo)) == FALSE)
				return FALSE;
		if((clp = lforw(clp)) == bp->b_linep) break;
		if (newline(FFRAND, 1) == FALSE) /* fake newline */
			return FALSE;
		nline++;
	}
	if (nline == 1) ewprintf("[Inserted 1 line]");
	else	ewprintf("[Inserted %d lines]", nline);

	clp = curwp->w_linep;			/* cosmetic adjustment */
	if (curwp->w_dotp == clp) {		/* for offscreen insert */
		while (nline-- && lback(clp)!=curbp->b_linep)
			clp = lback(clp);
		curwp->w_linep = clp;		/* adjust framing.	*/
		curwp->w_flag |= WFHARD;
	}
	return (TRUE);
}

/*
 * Turn off the dirty bit on this buffer.
 */
/*ARGSUSED*/
notmodified(f, n)
{
	register MGWIN *wp;

	curbp->b_flag &= ~BFCHG;
	wp = wheadp;				/* Update mode lines.	*/
	while (wp != NULL) {
		if (wp->w_bufp == curbp)
			wp->w_flag |= WFMODE;
		wp = wp->w_wndp;
	}
	ewprintf("Modification-flag cleared");
	return TRUE;
}

#ifndef NO_HELP
/*
 * Popbuf and set all windows to top of buffer.	 Currently only used by
 * help functions.
 */

popbuftop(bp)
register BUFFER *bp;
{
    register MGWIN *wp;

    bp->b_dotp = lforw(bp->b_linep);
    bp->b_doto = 0;
    if(bp->b_nwnd != 0) {
	for(wp = wheadp; wp!=NULL; wp = wp->w_wndp)
	    if(wp->w_bufp == bp) {
		wp->w_dotp = bp->b_dotp;
		wp->w_doto = 0;
		wp->w_flag |= WFHARD;
	    }
    }
    return popbuf(bp) != NULL;
}
#endif
