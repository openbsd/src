/*
 *		File commands.
 */
#include	"def.h"

BUFFER	*findbuffer();
VOID	makename();
VOID	upmodes();
static	char *itos();

/*
 * insert a file into the current buffer. Real easy - just call the
 * insertfile routine with the file name.
 */
/*ARGSUSED*/
fileinsert(f, n)
{
	register int	s;
	char		fname[NFILEN];

	if ((s=eread("Insert file: ", fname, NFILEN, EFNEW|EFCR|EFFILE)) != TRUE)
		return (s);
	return insertfile(adjustname(fname), (char *) NULL, FALSE);
						/* don't set buffer name */
}

/*
 * Select a file for editing.
 * Look around to see if you can find the
 * fine in another buffer; if you can find it
 * just switch to the buffer. If you cannot find
 * the file, create a new buffer, read in the
 * text, and switch to the new buffer.
 */
/*ARGSUSED*/
filevisit(f, n)
{
	register BUFFER *bp;
	int	s;
	char	fname[NFILEN];
	char	*adjf;

	if ((s=eread("Find file: ", fname, NFILEN, EFNEW|EFCR|EFFILE)) != TRUE)
		return s;
	adjf = adjustname(fname);
	if ((bp = findbuffer(adjf)) == NULL) return FALSE;
	curbp = bp;
	if (showbuffer(bp, curwp, WFHARD) != TRUE) return FALSE;
	if (bp->b_fname[0] == 0)
		return readin(adjf);		/* Read it in.		*/
	return TRUE;
}

/*
 * Pop to a file in the other window. Same as last function, just
 * popbuf instead of showbuffer.
 */
/*ARGSUSED*/
poptofile(f, n)
{
	register BUFFER *bp;
	register MGWIN *wp;
	int	s;
	char	fname[NFILEN];
	char	*adjf;

	if ((s=eread("Find file in other window: ", fname, NFILEN,
		     EFNEW|EFCR|EFFILE)) != TRUE)
		return s;
	adjf = adjustname(fname);
	if ((bp = findbuffer(adjf)) == NULL) return FALSE;
	if ((wp = popbuf(bp)) == NULL) return FALSE;
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == 0)
		return readin(adjf);		/* Read it in.		*/
	return TRUE;
}

/*
 * given a file name, either find the buffer it uses, or create a new
 * empty buffer to put it in.
 */
BUFFER *
findbuffer(fname)
char *fname;
{
	register BUFFER *bp;
	char	bname[NBUFN], *cp;
	unsigned count = 1;

	for (bp=bheadp; bp!=NULL; bp=bp->b_bufp) {
		if (fncmp(bp->b_fname, fname) == 0)
			return bp;
	}
	makename(bname, fname);			/* New buffer name.	*/
	cp = bname + strlen(bname);
	while(bfind(bname, FALSE) != NULL) {
		*cp = '<';		/* add "<count>" to then name	*/
		(VOID) strcpy(itos(cp, ++count)+1, ">");
	}
	return bfind(bname, TRUE);
}

/*
 * Put the decimal representation of num into a buffer.  Hacked to be
 * faster, smaller, and less general.
 */
static char *itos(bufp, num)
char *bufp;
unsigned num;
{
	if (num >= 10) {
		bufp = itos(bufp, num/10);
		num %= 10;
	}
	*++bufp = '0' + num;
	return bufp;
}

/*
 * Read the file "fname" into the current buffer.
 * Make all of the text in the buffer go away, after checking
 * for unsaved changes. This is called by the "read" command, the
 * "visit" command, and the mainline (for "uemacs file").
 */
readin(fname) char *fname; {
	register int		status;
	register MGWIN		*wp;

	if (bclear(curbp) != TRUE)		/* Might be old.	*/
		return TRUE;
	status = insertfile(fname, fname, TRUE) ;
	curbp->b_flag &= ~BFCHG;		/* No change.		*/
	for (wp=wheadp; wp!=NULL; wp=wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_dotp  = wp->w_linep = lforw(curbp->b_linep);
			wp->w_doto  = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
		}
	}
	return status;
}
/*
 * NB, getting file attributes is done here under control of a flag
 * rather than in readin, which would be cleaner.  I was concerned
 * that some operating system might require the file to be open
 * in order to get the information.  Similarly for writing.
 */

/*
 * insert a file in the current buffer, after dot. Set mark
 * at the end of the text inserted, point at the beginning.
 * Return a standard status. Print a summary (lines read,
 * error message) out as well. If the
 * BACKUP conditional is set, then this routine also does the read
 * end of backup processing. The BFBAK flag, if set in a buffer,
 * says that a backup should be taken. It is set when a file is
 * read in, but not on a new file (you don't need to make a backup
 * copy of nothing).
 */
static char *line = NULL;
static int linesize = 0;

insertfile(fname, newname, needinfo) char fname[], newname[]; {
	register LINE	*lp1;
	register LINE	*lp2;
	register MGWIN *wp;
	int		nbytes;
	LINE		*olp;			/* Line we started at */
	int		opos;			/* and offset into it */
	int		s, nline;
	BUFFER		*bp;

	if (line == NULL) {
		line = malloc(NLINE);
		linesize = NLINE;
	}
	bp = curbp;				/* Cheap.		*/
	if (newname != (char *) NULL)
		(VOID) strcpy(bp->b_fname, newname);
	/* Hard file open.	*/
	if ((s=ffropen(fname, needinfo ? bp : (BUFFER *) NULL)) == FIOERR)
		goto out;
	if (s == FIOFNF) {			/* File not found.	*/
		if (newname != NULL)
			ewprintf("(New file)");
		else	ewprintf("(File not found)");
		goto out;
	}
	opos = curwp->w_doto;
	/* Open a new line, at point, and start inserting after it */
	(VOID) lnewline();
	olp = lback(curwp->w_dotp);
	if(olp == curbp->b_linep) {
		/* if at end of buffer, create a line to insert before */
		(VOID) lnewline();
		curwp->w_dotp = lback(curwp->w_dotp);
	}
	nline = 0;			/* Don't count fake line at end */
	while ((s=ffgetline(line, linesize, &nbytes)) != FIOERR) {
doneread:
	    switch(s) {
	    case FIOSUC:
		++nline;
		/* and continue */
	    case FIOEOF:	/* the last line of the file		*/
		if ((lp1=lalloc(nbytes)) == NULL) {
			s = FIOERR;		/* Keep message on the	*/
			goto endoffile;		/* display.		*/
		}
		bcopy(line, &ltext(lp1)[0], nbytes);
		lp2 = lback(curwp->w_dotp);
		lp2->l_fp = lp1;
		lp1->l_fp = curwp->w_dotp;
		lp1->l_bp = lp2;
		curwp->w_dotp->l_bp = lp1;
		if(s==FIOEOF) goto endoffile;
		break;
	    case FIOLONG: {	/* a line to long to fit in our buffer	*/
		    char *cp;
		    int newsize;

		    newsize = linesize * 2;
		    if(newsize < 0 || 
		       (cp = malloc((unsigned)newsize)) == NULL) {
			    ewprintf("Could not allocate %d bytes",
				    newsize);
			    s = FIOERR;
			    goto endoffile;
		    }
		    bcopy(line, cp, linesize);
		    free(line);
		    line = cp;
		    s=ffgetline(line+linesize, linesize, &nbytes);
		    nbytes += linesize;
		    linesize = newsize;
		    if (s == FIOERR)
			goto endoffile;
		    goto doneread;
		}
	    default:
		ewprintf("Unknown code %d reading file", s);
		s = FIOERR;
		break;
	    }
	}
endoffile:
	(VOID) ffclose((BUFFER *) NULL);	/* Ignore errors.	*/
	if (s==FIOEOF) {			/* Don't zap an error.	*/
		if (nline == 1) ewprintf("(Read 1 line)");
		else		ewprintf("(Read %d lines)", nline);
	}
	/* Set mark at the end of the text */
	curwp->w_dotp = curwp->w_markp = lback(curwp->w_dotp);
	curwp->w_marko = llength(curwp->w_markp);
	(VOID) ldelnewline();
	curwp->w_dotp = olp;
	curwp->w_doto = opos;
	if(olp == curbp->b_linep) curwp->w_dotp = lforw(olp);
#ifndef NO_BACKUP
	if (newname != NULL)
		bp->b_flag |= BFCHG | BFBAK;	/* Need a backup.	*/
	else	bp->b_flag |= BFCHG;
#else
	bp->b_flag |= BFCHG;
#endif
	/* if the insert was at the end of buffer, set lp1 to the end of
	 * buffer line, and lp2 to the beginning of the newly inserted
	 * text.  (Otherwise lp2 is set to NULL.)  This is
	 * used below to set pointers in other windows correctly if they
	 * are also at the end of buffer.
	 */
	lp1 = bp->b_linep;
	if (curwp->w_markp == lp1) {
		lp2 = curwp->w_dotp;
	} else {
		(VOID) ldelnewline();		/* delete extranious newline */
out:		lp2 = NULL;
	}
	for (wp=wheadp; wp!=NULL; wp=wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_flag |= WFMODE|WFEDIT;
			if (wp != curwp && lp2 != NULL) {
				if (wp->w_dotp == lp1)	wp->w_dotp  = lp2;
				if (wp->w_markp == lp1) wp->w_markp = lp2;
				if (wp->w_linep == lp1) wp->w_linep = lp2;
			}
		}
	}
	return s != FIOERR;			/* False if error.	*/
}

/*
 * Take a file name, and from it
 * fabricate a buffer name. This routine knows
 * about the syntax of file names on the target system.
 * BDC1		left scan delimiter.
 * BDC2		optional second left scan delimiter.
 * BDC3		optional right scan delimiter.
 */
VOID
makename(bname, fname) char bname[]; char fname[]; {
	register char	*cp1;
	register char	*cp2;

	cp1 = &fname[0];
	while (*cp1 != 0)
		++cp1;
	--cp1;			/* insure at least 1 character ! */
#ifdef	BDC2
	while (cp1!=&fname[0] && cp1[-1]!=BDC1 && cp1[-1]!=BDC2)
		--cp1;
#else
	while (cp1!=&fname[0] && cp1[-1]!=BDC1)
		--cp1;
#endif
	cp2 = &bname[0];
#ifdef	BDC3
	while (cp2!=&bname[NBUFN-1] && *cp1!=0 && *cp1!=BDC3)
		*cp2++ = *cp1++;
#else
	while (cp2!=&bname[NBUFN-1] && *cp1!=0)
		*cp2++ = *cp1++;
#endif
	*cp2 = 0;
}

/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 * Update the remembered file name and clear the
 * buffer changed flag. This handling of file names
 * is different from the earlier versions, and
 * is more compatable with Gosling EMACS than
 * with ITS EMACS.
 */
/*ARGSUSED*/
filewrite(f, n)
{
	register int	s;
	char		fname[NFILEN];
	char		*adjfname;

	if ((s=eread("Write file: ", fname, NFILEN,
		      EFNEW|EFCR|EFFILE)) != TRUE)
		return (s);
	adjfname = adjustname(fname);
	/* old attributes are no longer current */
	bzero(&curbp->b_fi, sizeof(curbp->b_fi));
	if ((s=writeout(curbp, adjfname)) == TRUE) {
		(VOID) strcpy(curbp->b_fname, adjfname);
#ifndef NO_BACKUP
		curbp->b_flag &= ~(BFBAK | BFCHG);
#else
		curbp->b_flag &= ~BFCHG;
#endif
		upmodes(curbp);
	}
	return s;
}

/*
 * Save the contents of the current buffer back into
 * its associated file.
 */
#ifndef NO_BACKUP
#ifndef	MAKEBACKUP
#define	MAKEBACKUP TRUE
#endif
static int	makebackup = MAKEBACKUP;
#endif

/*ARGSUSED*/
filesave(f, n)
{
	return buffsave(curbp);
}

/*
 * Save the contents of the buffer argument into its associated file.
 * Do nothing if there have been no changes
 * (is this a bug, or a feature). Error if there is no remembered
 * file name. If this is the first write since the read or visit,
 * then a backup copy of the file is made.
 * Allow user to select whether or not to make backup files
 * by looking at the value of makebackup.
 */
buffsave(bp) BUFFER *bp; {
	register int	s;

	if ((bp->b_flag&BFCHG) == 0)	{	/* Return, no changes.	*/
		ewprintf("(No changes need to be saved)");
		return TRUE;
	}
	if (bp->b_fname[0] == '\0') {		/* Must have a name.	*/
		ewprintf("No file name");
		return (FALSE);
	}
#ifndef NO_BACKUP
	if (makebackup && (bp->b_flag&BFBAK)) {
		s = fbackupfile(bp->b_fname);
		if (s == ABORT)			/* Hard error.		*/
			return FALSE;
		if (s == FALSE			/* Softer error.	*/
		&& (s=eyesno("Backup error, save anyway")) != TRUE)
			return s;
	}
#endif
	if ((s=writeout(bp, bp->b_fname)) == TRUE) {
#ifndef NO_BACKUP
		bp->b_flag &= ~(BFCHG | BFBAK);
#else
		bp->b_flag &= ~BFCHG;
#endif
		upmodes(bp);
	}
	return s;
}

#ifndef NO_BACKUP
/* Since we don't have variables (we probably should)
 * this is a command processor for changing the value of
 * the make backup flag.  If no argument is given,
 * sets makebackup to true, so backups are made.  If
 * an argument is given, no backup files are made when
 * saving a new version of a file. Only used when BACKUP
 * is #defined.
 */
/*ARGSUSED*/
makebkfile(f, n)
{
	if(f & FFARG) makebackup = n > 0;
	else makebackup = !makebackup;
	ewprintf("Backup files %sabled", makebackup ? "en" : "dis");
	return TRUE;
}
#endif

/*
 * NB: bp is passed to both ffwopen and ffclose because some
 * attribute information may need to be updated at open time
 * and others after the close.  This is OS-dependent.  Note
 * that the ff routines are assumed to be able to tell whether
 * the attribute information has been set up in this buffer
 * or not.
 */

/*
 * This function performs the details of file
 * writing; writing the file in buffer bp to
 * file fn. Uses the file management routines
 * in the "fileio.c" package. Most of the grief
 * is checking of some sort.
 */
writeout(bp, fn) register BUFFER *bp; char *fn; {
	register int	s;

	if ((s=ffwopen(fn,bp)) != FIOSUC)	/* Open writes message. */
		return (FALSE);
	s = ffputbuf(bp);
	if (s == FIOSUC) {			/* No write error.	*/
		s = ffclose(bp);
		if (s==FIOSUC)
			ewprintf("Wrote %s", fn);
	} else					/* Ignore close error	*/
		(VOID) ffclose(bp);		/* if a write error.	*/
	return s == FIOSUC;
}

/*
 * Tag all windows for bp (all windows if bp NULL) as needing their
 * mode line updated.
 */
VOID
upmodes(bp) register BUFFER *bp; {
	register MGWIN *wp;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (bp == NULL || curwp->w_bufp == bp) wp->w_flag |= WFMODE;
}
