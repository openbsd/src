/*
 *		Echo line reading and writing.
 *
 * Common routines for reading
 * and writing characters in the echo line area
 * of the display screen. Used by the entire
 * known universe.
 */
/*
 * The varargs lint directive comments are 0 an attempt to get lint to shup
 * up about CORRECT usage of varargs.h.  It won't.
 */
#include	"def.h"
#include	"key.h"
#ifdef	LOCAL_VARARGS
#include	"varargs.h"
#else
#include	<varargs.h>
#endif
#ifndef NO_MACRO
#  include	"macro.h"
#endif

static int	veread();
VOID		ewprintf();
static VOID	eformat();
static VOID	eputi();
static VOID	eputl();
static VOID	eputs();
static VOID	eputc();
static int	complt();
static int	complt_list();
LIST *		make_file_list();
LIST *		copy_list();
char *		strrchr();
extern LIST *	complete_function_list();

int	epresf	= FALSE;		/* Stuff in echo line flag.	*/

extern		int tthue;

/*
 * Erase the echo line.
 */
VOID
eerase() {
	ttcolor(CTEXT);
	ttmove(nrow-1, 0);
	tteeol();
	ttflush();
	epresf = FALSE;
}

/*
 * Ask "yes" or "no" question.
 * Return ABORT if the user answers the question
 * with the abort ("^G") character. Return FALSE
 * for "no" and TRUE for "yes". No formatting
 * services are available. No newline required.
 */
eyorn(sp) char *sp; {
	register int	s;

#ifndef NO_MACRO
	if(inmacro) return TRUE;
#endif
	ewprintf("%s? (y or n) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y') return TRUE;
		if (s == 'n' || s == 'N') return FALSE;
		if (s == CCHR('G')) return ctrlg(FFRAND, 1);
		ewprintf("Please answer y or n.  %s? (y or n) ", sp);
	}
	/*NOTREACHED*/
}

/*
 * Like eyorn, but for more important question. User must type either all of
 * "yes" or "no", and the trainling newline.
 */
eyesno(sp) char *sp; {
	register int	s;
	char		buf[64];

#ifndef NO_MACRO
	if(inmacro) return TRUE;
#endif
	s = ereply("%s? (yes or no) ", buf, sizeof(buf), sp);
	for (;;) {
		if (s == ABORT) return ABORT;
		if (s != FALSE) {
#ifndef NO_MACRO
			if (macrodef) {
			    LINE *lp = maclcur;

			    maclcur = lp->l_bp;
			    maclcur->l_fp = lp->l_fp;
			    free((char *)lp);
			}
#endif
			if ((buf[0] == 'y' || buf[0] == 'Y')
			    &&	(buf[1] == 'e' || buf[1] == 'E')
			    &&	(buf[2] == 's' || buf[2] == 'S')
			    &&	(buf[3] == '\0')) return TRUE;
			if ((buf[0] == 'n' || buf[0] == 'N')
			    &&	(buf[1] == 'o' || buf[0] == 'O')
			    &&	(buf[2] == '\0')) return FALSE;
		}
		s = ereply("Please answer yes or no.  %s? (yes or no) ",
			   buf, sizeof(buf), sp);
	}
	/*NOTREACHED*/
}
/*
 * Write out a prompt, and read back a
 * reply. The prompt is now written out with full "ewprintf"
 * formatting, although the arguments are in a rather strange
 * place. This is always a new message, there is no auto
 * completion, and the return is echoed as such.
 */
/*VARARGS 0*/
ereply(va_alist)
va_dcl
{
	va_list pvar;
	register char *fp, *buf;
	register int nbuf;
	register int i;

	va_start(pvar);
	fp = va_arg(pvar, char *);
	buf = va_arg(pvar, char *);
	nbuf = va_arg(pvar, int);
	i = veread(fp, buf, nbuf, EFNEW|EFCR, &pvar);
	va_end(pvar);
	return i;
}

/*
 * This is the general "read input from the
 * echo line" routine. The basic idea is that the prompt
 * string "prompt" is written to the echo line, and a one
 * line reply is read back into the supplied "buf" (with
 * maximum length "len"). The "flag" contains EFNEW (a
 * new prompt), an EFFUNC (autocomplete), or EFCR (echo
 * the carriage return as CR).
 */
/* VARARGS 0 */
eread(va_alist)
va_dcl
{
	va_list pvar;
	char *fp, *buf;
	int nbuf, flag, i;
	va_start(pvar);
	fp   = va_arg(pvar, char *);
	buf  = va_arg(pvar, char *);
	nbuf = va_arg(pvar, int);
	flag = va_arg(pvar, int);
	i = veread(fp, buf, nbuf, flag, &pvar);
	va_end(pvar);
	return i;
}

static veread(fp, buf, nbuf, flag, ap) char *fp; char *buf; va_list *ap; {
	register int	cpos;
	register int	i;
	register int	c;

#ifndef NO_MACRO
	if(inmacro) {
	    bcopy(maclcur->l_text, buf, maclcur->l_used);
	    buf[maclcur->l_used] = '\0';
	    maclcur = maclcur->l_fp;
	    return TRUE;
	}
#endif
	cpos = 0;
	if ((flag&EFNEW)!=0 || ttrow!=nrow-1) {
		ttcolor(CTEXT);
		ttmove(nrow-1, 0);
		epresf = TRUE;
	} else
		eputc(' ');
	eformat(fp, ap);
	tteeol();
	ttflush();
	for (;;) {
		c = getkey(FALSE);
		if ((flag&EFAUTO) != 0 && (c == ' ' || c == CCHR('I'))) {
			cpos += complt(flag, c, buf, cpos);
			continue;
		}
		if ((flag&EFAUTO) != 0 && c == '?') {
			complt_list(flag, c, buf, cpos);
			continue;
		}
		switch (c) {
		    case CCHR('J'):
			c = CCHR('M');		/* and continue		*/
		    case CCHR('M'):		/* Return, done.	*/
			if ((flag&EFFUNC) != 0) {
				if ((i = complt(flag, c, buf, cpos)) == 0)
					continue;
				if (i > 0) cpos += i;
			}
			buf[cpos] = '\0';
			if ((flag&EFCR) != 0) {
				ttputc(CCHR('M'));
				ttflush();
			}
#ifndef NO_MACRO
			if(macrodef) {
			    LINE *lp;

			    if((lp = lalloc(cpos)) == NULL) return FALSE;
			    lp->l_fp = maclcur->l_fp;
			    maclcur->l_fp = lp;
			    lp->l_bp = maclcur;
			    maclcur = lp;
			    bcopy(buf, lp->l_text, cpos);
			}
#endif
			goto done;

		    case CCHR('G'):		/* Bell, abort.		*/
			eputc(CCHR('G'));
			(VOID) ctrlg(FFRAND, 0);
			ttflush();
			return ABORT;

		    case CCHR('H'):
		    case CCHR('?'):		/* Rubout, erase.	*/
			if (cpos != 0) {
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
				ttflush();
			}
			break;

		    case CCHR('X'):		/* C-X			*/
		    case CCHR('U'):		/* C-U, kill line.	*/
			while (cpos != 0) {
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
			}
			ttflush();
			break;

		    case CCHR('W'):		/* C-W, kill to beginning of */
						/* previous word	*/
			/* back up to first word character or beginning */
			while ((cpos > 0) && !ISWORD(buf[cpos - 1])) {
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
			}
			while ((cpos > 0) && ISWORD(buf[cpos - 1])) {
				ttputc('\b');
				ttputc(' ');
				ttputc('\b');
				--ttcol;
				if (ISCTRL(buf[--cpos]) != FALSE) {
					ttputc('\b');
					ttputc(' ');
					ttputc('\b');
					--ttcol;
				}
			}
			ttflush();
			break;

		    case CCHR('\\'):
		    case CCHR('Q'):		/* C-Q, quote next	*/
			c = getkey(FALSE);	/* and continue		*/
		    default:			/* All the rest.	*/
			if (cpos < nbuf-1) {
				buf[cpos++] = (char) c;
				eputc((char) c);
				ttflush();
			}
		}
	}
done:	return buf[0] != '\0';
}

/*
 * do completion on a list of objects.
 */
static int complt(flags, c, buf, cpos)
register char *buf;
register int cpos;
{
	register LIST	*lh, *lh2;
	LIST		*wholelist = NULL;
	int		i, nxtra;
	int		nhits, bxtra;
	int		wflag = FALSE;
	int		msglen, nshown;
	char		*msg;

	if ((flags&EFFUNC) != 0) {
	    buf[cpos] = '\0';
	    i = complete_function(buf, c);
	    if(i>0) {
		eputs(&buf[cpos]);
		ttflush();
		return i;
	    }
	    switch(i) {
		case -3:
		    msg = " [Ambiguous]";
		    break;
		case -2:
		    i=0;
		    msg = " [No match]";
		    break;
		case -1:
		case 0:
		    return i;
		default:
		    msg = " [Internal error]";
		    break;
	    }
	} else {
	    if ((flags&EFBUF) != 0) lh = &(bheadp->b_list);
	    else if ((flags&EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf,0);
	    }
	    else panic("broken complt call: flags");

	    if (c == ' ') wflag = TRUE;
	    else if (c != '\t' && c != CCHR('M')) panic("broken complt call: c");

	    nhits = 0;
	    nxtra = HUGE;

	    while (lh != NULL) {
		for (i=0; i<cpos; ++i) {
			if (buf[i] != lh->l_name[i])
				break;
		}
		if (i == cpos) {
			if (nhits == 0)
				lh2 = lh;
			++nhits;
			if (lh->l_name[i] == '\0') nxtra = -1;
			else {
				bxtra = getxtra(lh, lh2, cpos, wflag);
				if (bxtra < nxtra) nxtra = bxtra;
				lh2 = lh;
			}
		}
		lh = lh->l_next;
	    }
	    if (nhits == 0)
		msg = " [No match]";
	    else if (nhits > 1 && nxtra == 0)
		msg = " [Ambiguous]";
	    else {		/* Got a match, do it to it */
		/*
		 * Being lazy - ought to check length, but all things
		 * autocompleted have known types/lengths.
		 */
		if (nxtra < 0 && nhits > 1 && c == ' ') nxtra = 1;
		for (i = 0; i < nxtra; ++i) {
			buf[cpos] = lh2->l_name[cpos];
			eputc(buf[cpos++]);
		}
		ttflush();
		free_file_list(wholelist);
		if (nxtra < 0 && c != CCHR('M')) return 0;
		return nxtra;
	    }
	}
	/* wholelist is null if we are doing buffers.  want to free
	 * lists that were created for us, but not the buffer list! */
	free_file_list(wholelist);
	/* Set up backspaces, etc., being mindful of echo line limit */
	msglen = strlen(msg);
	nshown = (ttcol + msglen + 2 > ncol) ?
			ncol - ttcol - 2 : msglen;
	eputs(msg);
	ttcol -= (i = nshown);		/* update ttcol!		*/
	while (i--)			/* move back before msg		*/
		ttputc('\b');
	ttflush();			/* display to user		*/
	i = nshown;
	while (i--)			/* blank out	on next flush	*/
		eputc(' ');
	ttcol -= (i = nshown);		/* update ttcol on BS's		*/
	while (i--)
		ttputc('\b');		/* update ttcol again!		*/
	return 0;
}

/*
 * do completion on a list of objects, listing instead of completing
 */
static int complt_list(flags, c, buf, cpos)
register char *buf;
register int cpos;
{
	register LIST	*lh, *lh2, *lh3;
	LIST		*wholelist = NULL;
	int		i,maxwidth,width;
	int		preflen = 0;
        BUFFER		*bp;
        static VOID	findbind();
	int		oldrow = ttrow;
	int		oldcol = ttcol;
	int		oldhue = tthue;
	char		linebuf[NCOL+1];
	char		*cp;

	ttflush();

	/* the results are put into a help buffer */

	bp = bfind("*help*", TRUE);
	if(bclear(bp) == FALSE) return FALSE;

	{  /* this {} present for historical reasons */

/*
 * first get the list of objects.  This list may contain only the
 * ones that complete what has been typed, or may be the whole list
 * of all objects of this type.  They are filtered later in any case.
 * set wholelist if the list has been cons'ed up just for us, so we
 * can free it later.  We have to copy the buffer list for this
 * function even though we didn't for complt.  The sorting code
 * does destructive changes to the list, which we don't want to
 * happen to the main buffer list!
 */
	    if ((flags&EFBUF) != 0)
		wholelist = lh = copy_list (&(bheadp->b_list));
	    else if ((flags&EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf, c);
	    }
	    else if ((flags&EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf,1);
		/*
		 * we don't want to display stuff up to the / for file names
		 * preflen is the list of a prefix of what the user typed
		 * that should not be displayed.		
		 */
		cp = strrchr(buf,'/');
		if (cp)
		    preflen = cp - buf + 1;
	    }
	    else panic("broken complt call: flags");


/* sort the list, since users expect to see it in alphabetic order */

 	    lh2 = lh;
	    while (lh2) {
	 	lh3 = lh2->l_next;
		while (lh3) {
		    if (strcmp(lh2->l_name, lh3->l_name) > 0) {
			cp = lh2->l_name;
			lh2->l_name = lh3->l_name;
			lh3->l_name = cp;
		    }
		    lh3 = lh3->l_next;
	    	}
	        lh2 = lh2->l_next;
	    }

/*
 * first find max width of object to be displayed, so we can
 * put several on a line
 */
	    maxwidth = 0;

	    lh2 = lh;		
	    while (lh2 != NULL) {
		for (i=0; i<cpos; ++i) {
			if (buf[i] != lh2->l_name[i])
				break;
		}
		if (i == cpos) {
			width = strlen(lh2->l_name);
			if (width > maxwidth)
				maxwidth = width;
		}
		lh2 = lh2->l_next;
	    }
	    maxwidth += 1 - preflen;

/*
 * now do the display.  objects are written into linebuf until it
 * fills, and then put into the help buffer.
 */
	    cp = linebuf;
	    width = 0;
	    lh2 = lh;
	    while (lh2 != NULL) {
		for (i=0; i<cpos; ++i) {
			if (buf[i] != lh2->l_name[i])
				break;
		}
		if (i == cpos) {
			if ((width + maxwidth) > ncol) {
				*cp = 0;
				addline(bp,linebuf);
				cp = linebuf;
				width = 0;
			}
			strcpy(cp,lh2->l_name+preflen);
			i = strlen(lh2->l_name+preflen);
			cp += i;
			for (; i < maxwidth; i++)
				*cp++ = ' ';
			width += maxwidth;
		}
		lh2 = lh2->l_next;
	    }
	    if (width > 0) {
		*cp = 0;
		addline(bp,linebuf);
	    }
 	}
	/* 
	 * note that we free lists only if they are put in wholelist
	 * lists that were built just for us should be freed.  However
	 * when we use the buffer list, obviously we don't want it
	 * freed.
	 */
	free_file_list(wholelist);
	popbuftop(bp);   /* split the screen and put up the help buffer */
	update();	 /* needed to make the new stuff actually appear */
	ttmove(oldrow,oldcol);  /* update leaves cursor in arbitrary place */
	ttcolor(oldhue);  /* with arbitrary color */
	ttflush();
	return 0;
}

/*
 * The "lp1" and "lp2" point to list structures. The
 * "cpos" is a horizontal position in the name.
 * Return the longest block of characters that can be
 * autocompleted at this point. Sometimes the two
 * symbols are the same, but this is normal.
  */
getxtra(lp1, lp2, cpos, wflag) register LIST *lp1, *lp2; register int wflag; {
	register int	i;

	i = cpos;
	for (;;) {
		if (lp1->l_name[i] != lp2->l_name[i]) break;
		if (lp1->l_name[i] == '\0') break;
		++i;
		if (wflag && !ISWORD(lp1->l_name[i-1])) break;
	}
	return (i - cpos);
}

/*
 * Special "printf" for the echo line.
 * Each call to "ewprintf" starts a new line in the
 * echo area, and ends with an erase to end of the
 * echo line. The formatting is done by a call
 * to the standard formatting routine.
 */
/*VARARGS 0 */
VOID
ewprintf(va_alist)
va_dcl
{
	va_list pvar;
	register char *fp;

#ifndef NO_MACRO
	if(inmacro) return;
#endif
	va_start(pvar);
	fp = va_arg(pvar, char *);
	ttcolor(CTEXT);
	ttmove(nrow-1, 0);
	eformat(fp, &pvar);
	va_end(pvar);
	tteeol();
	ttflush();
	epresf = TRUE;
}

/*
 * Printf style formatting. This is
 * called by both "ewprintf" and "ereply" to provide
 * formatting services to their clients. The move to the
 * start of the echo line, and the erase to the end of
 * the echo line, is done by the caller.
 * Note: %c works, and prints the "name" of the character.
 * %k prints the name of a key (and takes no arguments).
 */
static VOID
eformat(fp, ap)
register char *fp;
register va_list *ap;
{
	register int c;
	char	kname[NKNAME];
	char	*keyname();
	char	*cp;

	while ((c = *fp++) != '\0') {
		if (c != '%')
			eputc(c);
		else {
			c = *fp++;
			switch (c) {
			case 'c':
				(VOID) keyname(kname, va_arg(*ap, int));
				eputs(kname);
				break;

			case 'k':
				cp = kname;
				for(c=0; c < key.k_count; c++) {
				    cp = keyname(cp, key.k_chars[c]);
				    *cp++ = ' ';
				}
				*--cp = '\0';
				eputs(kname);
				break;

			case 'd':
				eputi(va_arg(*ap, int), 10);
				break;

			case 'o':
				eputi(va_arg(*ap, int), 8);
				break;

			case 's':
				eputs(va_arg(*ap, char *));
				break;

			case 'l':/* explicit longword */
				c = *fp++;
				switch(c) {
				case 'd':
					eputl((long)va_arg(*ap, long), 10);
					break;
				default:
					eputc(c);
					break;
				}
				break;

			default:
				eputc(c);
			}
		}
	}
}

/*
 * Put integer, in radix "r".
 */
static VOID
eputi(i, r)
register int i;
register int r;
{
	register int	q;

	if(i<0) {
	    eputc('-');
	    i = -i;
	}
	if ((q=i/r) != 0)
		eputi(q, r);
	eputc(i%r+'0');
}

/*
 * Put long, in radix "r".
 */
static VOID
eputl(l, r)
register long l;
register int  r;
{
	register long	q;

	if(l < 0) {
	    eputc('-');
	    l = -l;
	}
	if ((q=l/r) != 0)
		eputl(q, r);
	eputc((int)(l%r)+'0');
}

/*
 * Put string.
 */
static VOID
eputs(s)
register char *s;
{
	register int	c;

	while ((c = *s++) != '\0')
		eputc(c);
}

/*
 * Put character. Watch for
 * control characters, and for the line
 * getting too long.
 */
static VOID
eputc(c)
register char c;
{
	if (ttcol+2 < ncol) {
		if (ISCTRL(c)) {
			eputc('^');
			c = CCHR(c);
		}
		ttputc(c);
		++ttcol;
	}
}

free_file_list(lp)
  LIST *lp;
{
LIST *next;
while (lp) {
	next = lp->l_next;
	free(lp);
	lp = next;
}
}

LIST *copy_list(lp)
  LIST *lp;
{
LIST *current,*last;

last = NULL;
while(lp) {
	current = (LIST *)malloc(sizeof(LIST));
	current->l_next = last;
	current->l_name = lp->l_name;
	last = (LIST *)current;
	lp = lp->l_next;
}
return(last);
}
