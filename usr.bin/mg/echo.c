/*	$OpenBSD: echo.c,v 1.25 2002/08/22 23:28:19 deraadt Exp $	*/

/*
 *	Echo line reading and writing.
 *
 * Common routines for reading and writing characters in the echo line area
 * of the display screen. Used by the entire known universe.
 */

#include "def.h"
#include "key.h"
#ifndef NO_MACRO
#include "macro.h"
#endif /* !NO_MACRO */

#include "funmap.h"

#include <stdarg.h>

static int	veread(const char *, char *buf, int, int, va_list);
static int	complt(int, int, char *, int);
static int	complt_list(int, int, char *, int);
static void	eformat(const char *, va_list);
static void	eputi(int, int);
static void	eputl(long, int);
static void	eputs(const char *);
static void	eputc(char);
static LIST	*copy_list(LIST *);

int		epresf = FALSE;		/* stuff in echo line flag */

/*
 * Erase the echo line.
 */
void
eerase(void)
{
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	tteeol();
	ttflush();
	epresf = FALSE;
}

/*
 * Ask a "yes" or "no" question.  Return ABORT if the user answers the
 * question with the abort ("^G") character.  Return FALSE for "no" and
 * TRUE for "yes".  No formatting services are available.  No newline
 * required.
 */
int
eyorn(const char *sp)
{
	int	 s;

#ifndef NO_MACRO
	if (inmacro)
		return TRUE;
#endif /* !NO_MACRO */
	ewprintf("%s? (y or n) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y')
			return TRUE;
		if (s == 'n' || s == 'N')
			return FALSE;
		if (s == CCHR('G'))
			return ctrlg(FFRAND, 1);
		ewprintf("Please answer y or n.  %s? (y or n) ", sp);
	}
	/* NOTREACHED */
}

/*
 * Like eyorn, but for more important questions.  User must type all of
 * "yes" or "no" and the trainling newline.
 */
int
eyesno(const char *sp)
{
	int	 s;
	char	 buf[64];

#ifndef NO_MACRO
	if (inmacro)
		return TRUE;
#endif /* !NO_MACRO */
	s = ereply("%s? (yes or no) ", buf, sizeof(buf), sp);
	for (;;) {
		if (s == ABORT)
			return ABORT;
		if (s != FALSE) {
#ifndef NO_MACRO
			if (macrodef) {
				LINE	*lp = maclcur;

				maclcur = lp->l_bp;
				maclcur->l_fp = lp->l_fp;
				free((char *)lp);
			}
#endif /* !NO_MACRO */
			if ((buf[0] == 'y' || buf[0] == 'Y') &&
			    (buf[1] == 'e' || buf[1] == 'E') &&
			    (buf[2] == 's' || buf[2] == 'S') &&
			    (buf[3] == '\0'))
				return TRUE;
			if ((buf[0] == 'n' || buf[0] == 'N') &&
			    (buf[1] == 'o' || buf[0] == 'O') &&
			    (buf[2] == '\0'))
				return FALSE;
		}
		s = ereply("Please answer yes or no.  %s? (yes or no) ",
		    buf, sizeof(buf), sp);
	}
	/* NOTREACHED */
}

/*
 * Write out a prompt and read back a reply.  The prompt is now written
 * out with full "ewprintf" formatting, although the arguments are in a
 * rather strange place.  This is always a new message, there is no auto
 * completion, and the return is echoed as such.
 */
/* VARARGS */
int
ereply(const char *fmt, char *buf, int nbuf, ...)
{
	va_list	 ap;
	int	 i;
	va_start(ap, nbuf);
	i = veread(fmt, buf, nbuf, EFNEW | EFCR, ap);
	va_end(ap);
	return i;
}

/*
 * This is the general "read input from the echo line" routine.  The basic
 * idea is that the prompt string "prompt" is written to the echo line, and
 * a one line reply is read back into the supplied "buf" (with maximum
 * length "len"). The "flag" contains EFNEW (a new prompt), an EFFUNC
 * (autocomplete), or EFCR (echo the carriage return as CR).
 */
/* VARARGS */
int
eread(const char *fmt, char *buf, int nbuf, int flag, ...)
{
	int	 i;
	va_list	 ap;
	va_start(ap, flag);
	i = veread(fmt, buf, nbuf, flag, ap);
	va_end(ap);
	return i;
}

static int
veread(const char *fp, char *buf, int nbuf, int flag, va_list ap)
{
	int	 cpos;
	int	 i;
	int	 c;

#ifndef NO_MACRO
	if (inmacro) {
		bcopy(maclcur->l_text, buf, maclcur->l_used);
		buf[maclcur->l_used] = '\0';
		maclcur = maclcur->l_fp;
		return TRUE;
	}
#endif /* !NO_MACRO */
	cpos = 0;
	if ((flag & EFNEW) != 0 || ttrow != nrow - 1) {
		ttcolor(CTEXT);
		ttmove(nrow - 1, 0);
		epresf = TRUE;
	} else
		eputc(' ');
	eformat(fp, ap);
	if ((flag & EFDEF) != 0) {
		eputs(buf);
		cpos += strlen(buf);
	}
	tteeol();
	ttflush();
	for (;;) {
		c = getkey(FALSE);
		if ((flag & EFAUTO) != 0 && (c == ' ' || c == CCHR('I'))) {
			cpos += complt(flag, c, buf, cpos);
			continue;
		}
		if ((flag & EFAUTO) != 0 && c == '?') {
			complt_list(flag, c, buf, cpos);
			continue;
		}
		switch (c) {
		case CCHR('J'):
			c = CCHR('M');
			/* FALLTHROUGH */
		case CCHR('M'):			/* return, done */
			if ((flag & EFFUNC) != 0) {
				if ((i = complt(flag, c, buf, cpos)) == 0)
					continue;
				if (i > 0)
					cpos += i;
			}
			buf[cpos] = '\0';
			if ((flag & EFCR) != 0) {
				ttputc(CCHR('M'));
				ttflush();
			}
#ifndef NO_MACRO
			if (macrodef) {
				LINE	*lp;

				if ((lp = lalloc(cpos)) == NULL)
					return FALSE;
				lp->l_fp = maclcur->l_fp;
				maclcur->l_fp = lp;
				lp->l_bp = maclcur;
				maclcur = lp;
				bcopy(buf, lp->l_text, cpos);
			}
#endif /* !NO_MACRO */
			goto done;
		case CCHR('G'):			/* bell, abort */
			eputc(CCHR('G'));
			(void)ctrlg(FFRAND, 0);
			ttflush();
			return ABORT;
		case CCHR('H'):			/* rubout, erase */
		case CCHR('?'):
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
		case CCHR('X'):			/* kill line */
		case CCHR('U'):
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
		case CCHR('W'):			/* kill to beginning of word */
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
		case CCHR('Q'):			/* quote next */
			c = getkey(FALSE);
			/* FALLTHROUGH */
		default:			/* all the rest */
			if (cpos < nbuf - 1) {
				buf[cpos++] = (char)c;
				eputc((char)c);
				ttflush();
			}
		}
	}
done:
	return buf[0] != '\0';
}

/*
 * do completion on a list of objects.
 */
static int
complt(int flags, int c, char *buf, int cpos)
{
	LIST	*lh, *lh2;
	LIST	*wholelist = NULL;
	int	 i, nxtra, nhits, bxtra, msglen, nshown;
	int	 wflag = FALSE;
	char	*msg;

	lh = lh2 = NULL;

	if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf, c);
	} else if ((flags & EFBUF) != 0) {
		lh = &(bheadp->b_list);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
	} else
		panic("broken complt call: flags");

	if (c == ' ')
		wflag = TRUE;
	else if (c != '\t' && c != CCHR('M'))
		panic("broken complt call: c");

	nhits = 0;
	nxtra = HUGE;

	for (; lh != NULL; lh = lh->l_next) {
		if (memcmp(buf, lh->l_name, cpos) != 0)
			continue;
		if (nhits == 0)
			lh2 = lh;
		++nhits;
		if (lh->l_name[cpos] == '\0')
			nxtra = -1;
		else {
			bxtra = getxtra(lh, lh2, cpos, wflag);
			if (bxtra < nxtra)
				nxtra = bxtra;
			lh2 = lh;
		}
	}
	if (nhits == 0)
		msg = " [No match]";
	else if (nhits > 1 && nxtra == 0)
		msg = " [Ambiguous]";
	else {
		/*
		 * Being lazy - ought to check length, but all things
		 * autocompleted have known types/lengths.
		 */
		if (nxtra < 0 && nhits > 1 && c == ' ')
			nxtra = 1;
		for (i = 0; i < nxtra; ++i) {
			buf[cpos] = lh2->l_name[cpos];
			eputc(buf[cpos++]);
		}
		ttflush();
		free_file_list(wholelist);
		if (nxtra < 0 && c != CCHR('M'))
			return 0;
		return nxtra;
	}

	/*
	 * wholelist is null if we are doing buffers.  want to free lists
	 * that were created for us, but not the buffer list!
	 */
	free_file_list(wholelist);

	/* Set up backspaces, etc., being mindful of echo line limit */
	msglen = strlen(msg);
	nshown = (ttcol + msglen + 2 > ncol) ?
		ncol - ttcol - 2 : msglen;
	eputs(msg);
	ttcol -= (i = nshown);	/* update ttcol!		 */
	while (i--)		/* move back before msg		 */
		ttputc('\b');
	ttflush();		/* display to user		 */
	i = nshown;
	while (i--)		/* blank out on next flush	 */
		eputc(' ');
	ttcol -= (i = nshown);	/* update ttcol on BS's		 */
	while (i--)
		ttputc('\b');	/* update ttcol again!		 */
	return 0;
}

/*
 * do completion on a list of objects, listing instead of completing
 */
static int
complt_list(int flags, int c, char *buf, int cpos)
{
	LIST	*lh, *lh2, *lh3;
	LIST	*wholelist = NULL;
	BUFFER	*bp;
	int	 i, maxwidth, width;
	int	 preflen = 0;
	int	 oldrow = ttrow;
	int	 oldcol = ttcol;
	int	 oldhue = tthue;
	char	 *linebuf;
	const char *cp;

	lh = NULL;

	ttflush();

	/* the results are put into a help buffer */
	bp = bfind("*help*", TRUE);
	if (bclear(bp) == FALSE)
		return FALSE;

	/*
	 * first get the list of objects.  This list may contain only
	 * the ones that complete what has been typed, or may be the
	 * whole list of all objects of this type.  They are filtered
	 * later in any case.  Set wholelist if the list has been
	 * cons'ed up just for us, so we can free it later.  We have
	 * to copy the buffer list for this function even though we
	 * didn't for complt.  The sorting code does destructive
	 * changes to the list, which we don't want to happen to the
	 * main buffer list!
	 */
	if ((flags & EFBUF) != 0)
		wholelist = lh = copy_list(&(bheadp->b_list));
	else if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf, c);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
		/*
		 * We don't want to display stuff up to the / for file
		 * names preflen is the list of a prefix of what the
		 * user typed that should not be displayed.
		 */
		cp = strrchr(buf, '/');
		if (cp)
			preflen = cp - buf + 1;
	} else
		panic("broken complt call: flags");


	/*
	 * Sort the list, since users expect to see it in alphabetic
	 * order.
	 */
	lh2 = lh;
	while (lh2 != NULL) {
		lh3 = lh2->l_next;
		while (lh3 != NULL) {
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
	 * First find max width of object to be displayed, so we can
	 * put several on a line.
	 */
	maxwidth = 0;
	lh2 = lh;
	while (lh2 != NULL) {
		for (i = 0; i < cpos; ++i) {
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
	 * Now do the display.  objects are written into linebuf until
	 * it fills, and then put into the help buffer.
	 */
	if ((linebuf = malloc((nrow + 1) * sizeof(char))) == NULL)
		return FALSE;
	width = 0;

	/*
	 * We're going to strlcat() into the buffer, so it has to be
	 * NUL terminated
	 */
	linebuf[0] = '\0';
	for (lh2 = lh; lh2 != NULL; lh2 = lh2->l_next) {
		for (i = 0; i < cpos; ++i) {
			if (buf[i] != lh2->l_name[i])
				break;
		}
		/* if we have a match */
		if (i == cpos) {
			/* if it wraps */
			if ((width + maxwidth) > ncol) {
				addline(bp, linebuf);
				linebuf[0] = '\0';
				width = 0;
			}
			strlcat(linebuf, lh2->l_name + preflen, nrow+1);
			i = strlen(lh2->l_name + preflen);
			/* make all the objects nicely line up */
			memset(linebuf + strlen(linebuf), ' ',
			    maxwidth - i);
			width += maxwidth;
			linebuf[width] = '\0';
		}
	}
	if (width > 0)
		addline(bp, linebuf);
	free(linebuf);

	/*
	 * Note that we free lists only if they are put in wholelist lists
	 * that were built just for us should be freed.  However when we use
	 * the buffer list, obviously we don't want it freed.
	 */
	free_file_list(wholelist);
	popbuftop(bp);		/* split the screen and put up the help
				 * buffer */
	update();		/* needed to make the new stuff actually
				 * appear */
	ttmove(oldrow, oldcol);	/* update leaves cursor in arbitrary place */
	ttcolor(oldhue);	/* with arbitrary color */
	ttflush();
	return 0;
}

/*
 * The "lp1" and "lp2" point to list structures.  The "cpos" is a horizontal
 * position in the name.  Return the longest block of characters that can be
 * autocompleted at this point.  Sometimes the two symbols are the same, but
 * this is normal.
 */
int
getxtra(LIST *lp1, LIST *lp2, int cpos, int wflag)
{
	int	i;

	i = cpos;
	for (;;) {
		if (lp1->l_name[i] != lp2->l_name[i])
			break;
		if (lp1->l_name[i] == '\0')
			break;
		++i;
		if (wflag && !ISWORD(lp1->l_name[i - 1]))
			break;
	}
	return (i - cpos);
}

/*
 * Special "printf" for the echo line.  Each call to "ewprintf" starts a
 * new line in the echo area, and ends with an erase to end of the echo
 * line.  The formatting is done by a call to the standard formatting
 * routine.
 */
/* VARARGS */
void
ewprintf(const char *fmt, ...)
{
	va_list	 ap;

#ifndef NO_MACRO
	if (inmacro)
		return;
#endif /* !NO_MACRO */
	va_start(ap, fmt);
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	eformat(fmt, ap);
	va_end(ap);
	tteeol();
	ttflush();
	epresf = TRUE;
}

/*
 * Printf style formatting. This is called by both "ewprintf" and "ereply"
 * to provide formatting services to their clients.  The move to the start
 * of the echo line, and the erase to the end of the echo line, is done by
 * the caller.
 * Note: %c works, and prints the "name" of the character.
 * %k prints the name of a key (and takes no arguments).
 */
static void
eformat(const char *fp, va_list ap)
{
	char	kname[NKNAME], *cp;
	int	c;

	while ((c = *fp++) != '\0') {
		if (c != '%')
			eputc(c);
		else {
			c = *fp++;
			switch (c) {
			case 'c':
				keyname(kname, sizeof(kname), va_arg(ap, int));
				eputs(kname);
				break;

			case 'k':
				for (cp = kname, c = 0; c < key.k_count; c++) {
					if (c)
						*cp++ = ' ';
					cp = keyname(cp, sizeof(kname) -
					    (cp - kname) - 1, key.k_chars[c]);
				}
				eputs(kname);
				break;

			case 'd':
				eputi(va_arg(ap, int), 10);
				break;

			case 'o':
				eputi(va_arg(ap, int), 8);
				break;

			case 's':
				eputs(va_arg(ap, char *));
				break;

			case 'l':
				/* explicit longword */
				c = *fp++;
				switch (c) {
				case 'd':
					eputl(va_arg(ap, long), 10);
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
static void
eputi(int i, int r)
{
	int	 q;

	if (i < 0) {
		eputc('-');
		i = -i;
	}
	if ((q = i / r) != 0)
		eputi(q, r);
	eputc(i % r + '0');
}

/*
 * Put long, in radix "r".
 */
static void
eputl(long l, int r)
{
	long	 q;

	if (l < 0) {
		eputc('-');
		l = -l;
	}
	if ((q = l / r) != 0)
		eputl(q, r);
	eputc((int)(l % r) + '0');
}

/*
 * Put string.
 */
static void
eputs(const char *s)
{
	int	 c;

	while ((c = *s++) != '\0')
		eputc(c);
}

/*
 * Put character.  Watch for control characters, and for the line getting
 * too long.
 */
static void
eputc(char c)
{
	if (ttcol + 2 < ncol) {
		if (ISCTRL(c)) {
			eputc('^');
			c = CCHR(c);
		}
		ttputc(c);
		++ttcol;
	}
}

void
free_file_list(LIST *lp)
{
	LIST	*next;

	while (lp) {
		next = lp->l_next;
		free(lp);
		lp = next;
	}
}

static LIST *
copy_list(LIST *lp)
{
	LIST	*current, *last, *nxt;

	last = NULL;
	while (lp) {
		current = (LIST *)malloc(sizeof(LIST));
		if (current == NULL) {
			for (current = last; current; current = nxt) {
				nxt = current->l_next;
				free(current);
				return (NULL);
			}
		}
		current->l_next = last;
		current->l_name = lp->l_name;
		last = (LIST *)current;
		lp = lp->l_next;
	}
	return (last);
}
