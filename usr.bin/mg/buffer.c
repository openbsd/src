/*	$OpenBSD: buffer.c,v 1.46 2005/10/11 01:08:52 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *		Buffer handling.
 */

#include "def.h"
#include "kbd.h"		/* needed for modes */
#include <stdarg.h>

static BUFFER  *makelist(void);

int
togglereadonly(int f, int n)
{
	if (!(curbp->b_flag & BFREADONLY)) {
		curbp->b_flag |= BFREADONLY;
		ewprintf("Now readonly");
	} else {
		curbp->b_flag &=~ BFREADONLY;
		if (curbp->b_flag & BFCHG)
			ewprintf("Warning: Buffer was modified");
	}
	curwp->w_flag |= WFMODE;

	return (1);
}

/*
 * Attach a buffer to a window. The values of dot and mark come
 * from the buffer if the use count is 0. Otherwise, they come
 * from some other window.  *scratch* is the default alternate
 * buffer.
 */
/* ARGSUSED */
int
usebuffer(int f, int n)
{
	BUFFER *bp;
	char    bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if ((curbp->b_altb == NULL) &&
	    ((curbp->b_altb = bfind("*scratch*", TRUE)) == NULL))
		bufp = eread("Switch to buffer: ", bufn, NBUFN, EFNEW | EFBUF);
	else
		bufp = eread("Switch to buffer: (default %s) ", bufn, NBUFN,
			  EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);

	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0' && curbp->b_altb != NULL)
		bp = curbp->b_altb;
	else if ((bp = bfind(bufn, TRUE)) == NULL)
		return (FALSE);

	/* and put it in current window */
	curbp = bp;
	return (showbuffer(bp, curwp, WFFORCE | WFHARD));
}

/*
 * pop to buffer asked for by the user.
 */
/* ARGSUSED */
int
poptobuffer(int f, int n)
{
	BUFFER *bp;
	MGWIN  *wp;
	char    bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if ((curbp->b_altb == NULL) &&
	    ((curbp->b_altb = bfind("*scratch*", TRUE)) == NULL))
		bufp = eread("Switch to buffer in other window: ", bufn, NBUFN,
			  EFNEW | EFBUF);
	else
		bufp = eread("Switch to buffer in other window: (default %s) ",
			bufn, NBUFN, EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0' && curbp->b_altb != NULL)
		bp = curbp->b_altb;
	else if ((bp = bfind(bufn, TRUE)) == NULL)
		return (FALSE);

	/* and put it in a new window */
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	return (TRUE);
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too
 * upset if it isn't there at all!). Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
/* ARGSUSED */
int
killbuffer_cmd(int f, int n)
{
	BUFFER *bp;
	char    bufn[NBUFN], *bufp;

	if ((bufp = eread("Kill buffer: (default %s) ", bufn, NBUFN,
	    EFNUL | EFNEW | EFBUF, curbp->b_bname)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		bp = curbp;
	else if ((bp = bfind(bufn, FALSE)) == NULL)
		return (FALSE);
	return (killbuffer(bp));
}

int
killbuffer(BUFFER *bp)
{
	BUFFER *bp1;
	BUFFER *bp2;
	MGWIN  *wp;
	int s;
	struct undo_rec *rec, *next;	

	/*
	 * Find some other buffer to display. Try the alternate buffer,
	 * then the first different buffer in the buffer list.  If there's
	 * only one buffer, create buffer *scratch* and make it the alternate
	 * buffer.  Return if *scratch* is only buffer...
	 */
	if ((bp1 = bp->b_altb) == NULL) {
		bp1 = (bp == bheadp) ? bp->b_bufp : bheadp;
		if (bp1 == NULL) {
			/* only one buffer. see if it's *scratch* */
			if (bp == bfind("*scratch*", FALSE))
				return (FALSE);
			/* create *scratch* for alternate buffer */
			if ((bp1 = bfind("*scratch*", TRUE)) == NULL)
				return (FALSE);
		}
	}
	if ((s = bclear(bp)) != TRUE)
		return (s);
	for (wp = wheadp; bp->b_nwnd > 0; wp = wp->w_wndp) {
		if (wp->w_bufp == bp) {
			bp2 = bp1->b_altb;	/* save alternate buffer */
			if (showbuffer(bp1, wp, WFMODE | WFFORCE | WFHARD))
				bp1->b_altb = bp2;
			else
				bp1 = bp2;
		}
	}
	if (bp == curbp)
		curbp = bp1;
	free(bp->b_linep);			/* Release header line.  */
	bp2 = NULL;				/* Find the header.	 */
	bp1 = bheadp;
	while (bp1 != bp) {
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp2 = bp1;
		bp1 = bp1->b_bufp;
	}
	bp1 = bp1->b_bufp;			/* Next one in chain.	 */
	if (bp2 == NULL)			/* Unlink it.		 */
		bheadp = bp1;
	else
		bp2->b_bufp = bp1;
	while (bp1 != NULL) {			/* Finish with altb's	 */
		if (bp1->b_altb == bp)
			bp1->b_altb = (bp->b_altb == bp1) ? NULL : bp->b_altb;
		bp1 = bp1->b_bufp;
	}
	rec = LIST_FIRST(&bp->b_undo);
	while (rec != NULL) {
		next = LIST_NEXT(rec, next);
		free_undo_record(rec);
		rec = next;
	}

	free((char *)bp->b_bname);		/* Release name block	 */
	free(bp);				/* Release buffer block */
	return (TRUE);
}

/*
 * Save some buffers - just call anycb with the arg flag.
 */
/* ARGSUSED */
int
savebuffers(int f, int n)
{
	if (anycb(f) == ABORT)
		return (ABORT);
	return (TRUE);
}

/*
 * Listing buffers.
 */
static int listbuf_ncol;

static int	listbuf_goto_buffer(int f, int n);
static int	listbuf_goto_buffer_one(int f, int n);
static int	listbuf_goto_buffer_helper(int f, int n, int only);

static PF listbuf_pf[] = {
	listbuf_goto_buffer
};
static PF listbuf_one[] = {
	listbuf_goto_buffer_one
};


static struct KEYMAPE (2 + IMAPEXT) listbufmap = {
	2,
	2 + IMAPEXT,
	rescan,
	{
		{
			CCHR('M'), CCHR('M'), listbuf_pf, NULL
		},
		{
			'1', '1', listbuf_one, NULL
		}
	}
};

/*
 * Display the buffer list. This is done
 * in two parts. The "makelist" routine figures out
 * the text, and puts it in a buffer. "popbuf"
 * then pops the data onto the screen. Bound to
 * "C-X C-B".
 */
/* ARGSUSED */
int
listbuffers(int f, int n)
{
	static int	 initialized = 0;
	BUFFER		*bp;
	MGWIN		*wp;

	if (!initialized) {
		maps_add((KEYMAP *)&listbufmap, "listbufmap");
		initialized = 1;
	}

	if ((bp = makelist()) == NULL || (wp = popbuf(bp)) == NULL)
		return (FALSE);
	wp->w_dotp = bp->b_dotp; /* fix up if window already on screen */
	wp->w_doto = bp->b_doto;
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("listbufmap");
	bp->b_nmodes = 1;

	return (TRUE);
}

/*
 * This routine rebuilds the text for the
 * list buffers command. Return pointer
 * to new list if everything works.
 * Return NULL if there is an error (if
 * there is no memory).
 */
static BUFFER *
makelist(void)
{
	int	w = ncol / 2;
	BUFFER *bp, *blp;
	LINE   *lp;


	if ((blp = bfind("*Buffer List*", TRUE)) == NULL)
		return (NULL);
	if (bclear(blp) != TRUE)
		return (NULL);
	blp->b_flag &= ~BFCHG;		/* Blow away old.	 */
	blp->b_flag |= BFREADONLY;

	listbuf_ncol = ncol;		/* cache ncol for listbuf_goto_buffer */

	if (addlinef(blp, "%-*s%s", w, " MR Buffer", "Size   File") == FALSE ||
	    addlinef(blp, "%-*s%s", w, " -- ------", "----   ----") == FALSE)
		return (NULL);

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		RSIZE nbytes;

		nbytes = 0;			/* Count bytes in buf.	 */
		if (bp != blp) {
			lp = lforw(bp->b_linep);
			while (lp != bp->b_linep) {
				nbytes += llength(lp) + 1;
				lp = lforw(lp);
			}
			if (nbytes)
				nbytes--;	/* no bonus newline	 */
		}

		if (addlinef(blp, "%c%c%c %-*.*s%c%-6d %-*s",
		    (bp == curbp) ? '.' : ' ',	/* current buffer ? */
		    ((bp->b_flag & BFCHG) != 0) ? '*' : ' ',	/* changed ? */
		    ((bp->b_flag & BFREADONLY) != 0) ? ' ' : '*',
		    w - 5,		/* four chars already written */
		    w - 5,		/* four chars already written */
		    bp->b_bname,	/* buffer name */
		    strlen(bp->b_bname) < w - 5 ? ' ' : '$', /* truncated? */
		    nbytes,		/* buffer size */
		    w - 7,		/* seven chars already written */
		    bp->b_fname) == FALSE)
			return (NULL);
	}
	blp->b_dotp = lforw(blp->b_linep);	/* put dot at beginning of
						 * buffer */
	blp->b_doto = 0;
	return (blp);				/* All done		 */
}

static int
listbuf_goto_buffer(int f, int n)
{
	return (listbuf_goto_buffer_helper(f, n, 0));
}

static int
listbuf_goto_buffer_one(int f, int n)
{
	return (listbuf_goto_buffer_helper(f, n, 1));
}

static int
listbuf_goto_buffer_helper(int f, int n, int only)
{
	BUFFER  *bp;
	MGWIN   *wp;
	char	*line = NULL;
	int	 i, ret = FALSE;

	if (curwp->w_dotp->l_text[listbuf_ncol/2 - 1] == '$') {
		ewprintf("buffer name truncated");
		return (FALSE);
	}

	if ((line = malloc(listbuf_ncol/2)) == NULL)
		return (FALSE);

	memcpy(line, curwp->w_dotp->l_text + 4, listbuf_ncol/2 - 5);
	for (i = listbuf_ncol/2 - 6; i > 0; i--) {
		if (line[i] != ' ') {
			line[i + 1] = '\0';
			break;
		}
	}
	if (i == 0)
		goto cleanup;

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (strcmp(bp->b_bname, line) == 0)
			break;
	}
	if (bp == NULL)
		goto cleanup;

	if ((wp = popbuf(bp)) == NULL)
		goto cleanup;
	curbp = bp;
	curwp = wp;

	if (only)
		ret = (onlywind(f, n));
	else
		ret = TRUE;

cleanup:
	free(line);

	return (ret);
}

/*
 * The argument "text" points to a format string.  Append this line to the
 * buffer. Handcraft the EOL on the end.  Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
int
addlinef(BUFFER *bp, char *fmt, ...)
{
	va_list  ap;
	LINE	*lp;

	if ((lp = lalloc(0)) == NULL)
		return (FALSE);
	va_start(ap, fmt);
	if (vasprintf(&lp->l_text, fmt, ap) == -1) {
		lfree(lp);
		va_end(ap);
		return (FALSE);
	}
	lp->l_used = strlen(lp->l_text);
	va_end(ap);

	bp->b_linep->l_bp->l_fp = lp;		/* Hook onto the end	 */
	lp->l_bp = bp->b_linep->l_bp;
	bp->b_linep->l_bp = lp;
	lp->l_fp = bp->b_linep;

	return (TRUE);
}

/*
 * Look through the list of buffers, giving the user a chance to save them.
 * Return TRUE if there are any changed buffers afterwards.  Buffers that
 * don't have an associated file don't count.  Return FALSE if there are
 * no changed buffers.
 */
int
anycb(int f)
{
	BUFFER *bp;
	int	s = FALSE, save = FALSE;
	char	prompt[NFILEN + 11];

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (bp->b_fname != NULL && *(bp->b_fname) != '\0' &&
		    (bp->b_flag & BFCHG) != 0) {
			snprintf(prompt, sizeof(prompt), "Save file %s",
			    bp->b_fname);
			if ((f == TRUE || (save = eyorn(prompt)) == TRUE) &&
			    buffsave(bp) == TRUE) {
				bp->b_flag &= ~BFCHG;
				upmodes(bp);
			} else
				s = TRUE;
			if (save == ABORT)
				return (save);
			save = TRUE;
		}
	}
	if (save == FALSE /* && kbdmop == NULL */ )	/* experimental */
		ewprintf("(No files need saving)");
	return (s);
}

/*
 * Search for a buffer, by name.
 * If not found, and the "cflag" is TRUE,
 * create a buffer and put it in the list of
 * all buffers. Return pointer to the BUFFER
 * block for the buffer.
 */
BUFFER *
bfind(const char *bname, int cflag)
{
	BUFFER	*bp;
	LINE	*lp;
	int	 i;

	bp = bheadp;
	while (bp != NULL) {
		if (strcmp(bname, bp->b_bname) == 0)
			return (bp);
		bp = bp->b_bufp;
	}
	if (cflag != TRUE)
		return (NULL);

	bp = calloc(1, sizeof(BUFFER));
	if (bp == NULL) {
		ewprintf("Can't get %d bytes", sizeof(BUFFER));
		return (NULL);
	}
	if ((bp->b_bname = strdup(bname)) == NULL) {
		ewprintf("Can't get %d bytes", strlen(bname) + 1);
		free(bp);
		return (NULL);
	}
	if ((lp = lalloc(0)) == NULL) {
		free((char *) bp->b_bname);
		free(bp);
		return (NULL);
	}
	bp->b_altb = bp->b_bufp = NULL;
	bp->b_dotp = lp;
	bp->b_doto = 0;
	bp->b_markp = NULL;
	bp->b_marko = 0;
	bp->b_flag = defb_flag;
	bp->b_nwnd = 0;
	bp->b_linep = lp;
	bp->b_nmodes = defb_nmodes;
	LIST_INIT(&bp->b_undo);
	bp->b_undoptr = NULL;
	memset(&bp->b_undopos, 0, sizeof(bp->b_undopos));
	i = 0;
	do {
		bp->b_modes[i] = defb_modes[i];
	} while (i++ < defb_nmodes);
	bp->b_fname[0] = '\0';
	bzero(&bp->b_fi, sizeof(bp->b_fi));
	lp->l_fp = lp;
	lp->l_bp = lp;
	bp->b_bufp = bheadp;
	bheadp = bp;
	return (bp);
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
int
bclear(BUFFER *bp)
{
	LINE  *lp;
	int    s;

	if ((bp->b_flag & BFCHG) != 0 &&	/* Changed. */
	    (s = eyesno("Buffer modified; kill anyway")) != TRUE)
		return (s);
	bp->b_flag &= ~BFCHG;	/* Not changed		 */
	while ((lp = lforw(bp->b_linep)) != bp->b_linep)
		lfree(lp);
	bp->b_dotp = bp->b_linep;	/* Fix dot */
	bp->b_doto = 0;
	bp->b_markp = NULL;	/* Invalidate "mark"	 */
	bp->b_marko = 0;
	return (TRUE);
}

/*
 * Display the given buffer in the given window. Flags indicated
 * action on redisplay.
 */
int
showbuffer(BUFFER *bp, MGWIN *wp, int flags)
{
	BUFFER *obp;
	MGWIN  *owp;

	if (wp->w_bufp == bp) {	/* Easy case! */
		wp->w_flag |= flags;
		wp->w_dotp = bp->b_dotp;
		wp->w_doto = bp->b_doto;
		return (TRUE);
	}
	/* First, detach the old buffer from the window */
	if ((bp->b_altb = obp = wp->w_bufp) != NULL) {
		if (--obp->b_nwnd == 0) {
			obp->b_dotp = wp->w_dotp;
			obp->b_doto = wp->w_doto;
			obp->b_markp = wp->w_markp;
			obp->b_marko = wp->w_marko;
		}
	}
	/* Now, attach the new buffer to the window */
	wp->w_bufp = bp;

	if (bp->b_nwnd++ == 0) {	/* First use.		 */
		wp->w_dotp = bp->b_dotp;
		wp->w_doto = bp->b_doto;
		wp->w_markp = bp->b_markp;
		wp->w_marko = bp->b_marko;
	} else
		/* already on screen, steal values from other window */
		for (owp = wheadp; owp != NULL; owp = wp->w_wndp)
			if (wp->w_bufp == bp && owp != wp) {
				wp->w_dotp = owp->w_dotp;
				wp->w_doto = owp->w_doto;
				wp->w_markp = owp->w_markp;
				wp->w_marko = owp->w_marko;
				break;
			}
	wp->w_flag |= WFMODE | flags;
	return (TRUE);
}

/*
 * Pop the buffer we got passed onto the screen.
 * Returns a status.
 */
MGWIN *
popbuf(BUFFER *bp)
{
	MGWIN  *wp;

	if (bp->b_nwnd == 0) {	/* Not on screen yet.	 */
		if ((wp = wpopup()) == NULL)
			return (NULL);
	} else
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_flag |= WFHARD | WFFORCE;
				return (wp);
			}
	if (showbuffer(bp, wp, WFHARD) != TRUE)
		return (NULL);
	return (wp);
}

/*
 * Insert another buffer at dot.  Very useful.
 */
/* ARGSUSED */
int
bufferinsert(int f, int n)
{
	BUFFER *bp;
	LINE   *clp;
	int	clo, nline;
	char	bufn[NBUFN], *bufp;

	/* Get buffer to use from user */
	if (curbp->b_altb != NULL)
		bufp = eread("Insert buffer: (default %s) ", bufn, NBUFN,
		    EFNUL | EFNEW | EFBUF, curbp->b_altb->b_bname);
	else
		bufp = eread("Insert buffer: ", bufn, NBUFN, EFNEW | EFBUF);
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0' && curbp->b_altb != NULL)
		bp = curbp->b_altb;
	else if ((bp = bfind(bufn, FALSE)) == NULL)
		return (FALSE);

	if (bp == curbp) {
		ewprintf("Cannot insert buffer into self");
		return (FALSE);
	}
	/* insert the buffer */
	nline = 0;
	clp = lforw(bp->b_linep);
	for (;;) {
		for (clo = 0; clo < llength(clp); clo++)
			if (linsert(1, lgetc(clp, clo)) == FALSE)
				return (FALSE);
		if ((clp = lforw(clp)) == bp->b_linep)
			break;
		if (newline(FFRAND, 1) == FALSE)	/* fake newline */
			return (FALSE);
		nline++;
	}
	if (nline == 1)
		ewprintf("[Inserted 1 line]");
	else
		ewprintf("[Inserted %d lines]", nline);

	clp = curwp->w_linep;		/* cosmetic adjustment	*/
	if (curwp->w_dotp == clp) {	/* for offscreen insert */
		while (nline-- && lback(clp) != curbp->b_linep)
			clp = lback(clp);
		curwp->w_linep = clp;	/* adjust framing.	*/
		curwp->w_flag |= WFHARD;
	}
	return (TRUE);
}

/*
 * Turn off the dirty bit on this buffer.
 */
/* ARGSUSED */
int
notmodified(int f, int n)
{
	MGWIN *wp;

	curbp->b_flag &= ~BFCHG;
	wp = wheadp;		/* Update mode lines.	 */
	while (wp != NULL) {
		if (wp->w_bufp == curbp)
			wp->w_flag |= WFMODE;
		wp = wp->w_wndp;
	}
	ewprintf("Modification-flag cleared");
	return (TRUE);
}

#ifndef NO_HELP
/*
 * Popbuf and set all windows to top of buffer.	 Currently only used by
 * help functions.
 */
int
popbuftop(BUFFER *bp)
{
	MGWIN *wp;

	bp->b_dotp = lforw(bp->b_linep);
	bp->b_doto = 0;
	if (bp->b_nwnd != 0) {
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_dotp = bp->b_dotp;
				wp->w_doto = 0;
				wp->w_flag |= WFHARD;
			}
	}
	return (popbuf(bp) != NULL);
}
#endif
