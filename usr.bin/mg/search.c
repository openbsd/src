/*	$OpenBSD: search.c,v 1.23 2005/10/11 01:00:41 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *		Search commands.
 * The functions in this file implement the search commands (both plain and
 * incremental searches are supported) and the query-replace command.
 *
 * The plain old search code is part of the original MicroEMACS "distribution".
 * The incremental search code and the query-replace code is by Rich Ellison.
 */

#include "def.h"
#include <ctype.h>

#ifndef NO_MACRO
#include "macro.h"
#endif /* !NO_MACRO */

#define SRCH_BEGIN	(0)	/* Search sub-codes.	 */
#define SRCH_FORW	(-1)
#define SRCH_BACK	(-2)
#define SRCH_NOPR	(-3)
#define SRCH_ACCM	(-4)
#define SRCH_MARK	(-5)

typedef struct {
	int	 s_code;
	LINE	*s_dotp;
	int	 s_doto;
} SRCHCOM;

static int	isearch(int);
static void	is_cpush(int);
static void	is_lpush(void);
static void	is_pop(void);
static int	is_peek(void);
static void	is_undo(int *, int *);
static int	is_find(int);
static void	is_prompt(int, int, int);
static void	is_dspl(char *, int);
static int	eq(int, int, int);

static SRCHCOM	cmds[NSRCH];
static int	cip;

int		srch_lastdir = SRCH_NOPR;	/* Last search flags.	 */

/*
 * Search forward.  Get a search string from the user, and search for it
 * starting at ".".  If found, "." gets moved to just after the matched
 * characters, and display does all the hard stuff.  If not found, it just
 * prints a message.
 */
/* ARGSUSED */
int
forwsearch(int f, int n)
{
	int	s;

	if ((s = readpattern("Search")) != TRUE)
		return (s);
	if (forwsrch() == FALSE) {
		ewprintf("Search failed: \"%s\"", pat);
		return (FALSE);
	}
	srch_lastdir = SRCH_FORW;
	return (TRUE);
}

/*
 * Reverse search.  Get a search string from the user, and search, starting
 * at "." and proceeding toward the front of the buffer.  If found "." is
 * left pointing at the first character of the pattern [the last character
 * that was matched].
 */
/* ARGSUSED */
int
backsearch(int f, int n)
{
	int	s;

	if ((s = readpattern("Search backward")) != TRUE)
		return (s);
	if (backsrch() == FALSE) {
		ewprintf("Search failed: \"%s\"", pat);
		return (FALSE);
	}
	srch_lastdir = SRCH_BACK;
	return (TRUE);
}

/*
 * Search again, using the same search string and direction as the last
 * search command. The direction has been saved in "srch_lastdir", so you
 * know which way to go.
 */
/* ARGSUSED */
int
searchagain(int f, int n)
{
	if (srch_lastdir == SRCH_FORW) {
		if (forwsrch() == FALSE) {
			ewprintf("Search failed: \"%s\"", pat);
			return (FALSE);
		}
		return (TRUE);
	}
	if (srch_lastdir == SRCH_BACK) {
		if (backsrch() == FALSE) {
			ewprintf("Search failed: \"%s\"", pat);
			return (FALSE);
		}
		return (TRUE);
	}
	ewprintf("No last search");
	return (FALSE);
}

/*
 * Use incremental searching, initially in the forward direction.
 * isearch ignores any explicit arguments.
 */
/* ARGSUSED */
int
forwisearch(int f, int n)
{
	return (isearch(SRCH_FORW));
}

/*
 * Use incremental searching, initially in the reverse direction.
 * isearch ignores any explicit arguments.
 */
/* ARGSUSED */
int
backisearch(int f, int n)
{
	return (isearch(SRCH_BACK));
}

/*
 * Incremental Search.
 *	dir is used as the initial direction to search.
 *	^S	switch direction to forward
 *	^R	switch direction to reverse
 *	^Q	quote next character (allows searching for ^N etc.)
 *	<ESC>	exit from Isearch
 *	<DEL>	undoes last character typed. (tricky job to do this correctly).
 *	other ^ exit search, don't set mark
 *	else	accumulate into search string
 */
static int
isearch(int dir)
{
	LINE	*clp;

	int	 c;
	int	 cbo;
	int	 success;
	int	 pptr;
	int	 firstc;
	int	 xcase;
	int	 i;
	char	 opat[NPAT];

#ifndef NO_MACRO
	if (macrodef) {
		ewprintf("Can't isearch in macro");
		return (FALSE);
	}
#endif /* !NO_MACRO */
	for (cip = 0; cip < NSRCH; cip++)
		cmds[cip].s_code = SRCH_NOPR;

	(void)strlcpy(opat, pat, sizeof(opat));
	cip = 0;
	pptr = -1;
	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	is_lpush();
	is_cpush(SRCH_BEGIN);
	success = TRUE;
	is_prompt(dir, TRUE, success);

	for (;;) {
		update();

		switch (c = getkey(FALSE)) {
		case CCHR('['):
			/*
			 * If new characters come in the next 300 msec,
			 * we can assume that they belong to a longer
			 * escaped sequence so we should ungetkey the
			 * ESC to avoid writing out garbage.
			 */
			if (ttwait(300) == FALSE)
				ungetkey(c);
			srch_lastdir = dir;
			curwp->w_markp = clp;
			curwp->w_marko = cbo;
			ewprintf("Mark set");
			return (TRUE);
		case CCHR('G'):
			if (success != TRUE) {
				while (is_peek() == SRCH_ACCM)
					is_undo(&pptr, &dir);
				success = TRUE;
				is_prompt(dir, pptr < 0, success);
				break;
			}
			curwp->w_dotp = clp;
			curwp->w_doto = cbo;
			curwp->w_flag |= WFMOVE;
			srch_lastdir = dir;
			(void)ctrlg(FFRAND, 0);
			(void)strlcpy(pat, opat, sizeof(pat));
			return (ABORT);
		case CCHR(']'):
		case CCHR('S'):
			if (dir == SRCH_BACK) {
				dir = SRCH_FORW;
				is_lpush();
				is_cpush(SRCH_FORW);
				success = TRUE;
			}
			if (success == FALSE && dir == SRCH_FORW) {
				/* wrap the search to beginning */
				clp = lforw(curbp->b_linep);
				curwp->w_dotp = clp;
				curwp->w_doto = 0;
				if (is_find(dir) != FALSE) {
					is_cpush(SRCH_MARK);
					success = TRUE;
				}
				break;
			}

			is_lpush();
			pptr = strlen(pat);
			(void)forwchar(FFRAND, 1);
			if (is_find(SRCH_FORW) != FALSE)
				is_cpush(SRCH_MARK);
			else {
				(void)backchar(FFRAND, 1);
				ttbeep();
				success = FALSE;
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('R'):
			if (dir == SRCH_FORW) {
				dir = SRCH_BACK;
				is_lpush();
				is_cpush(SRCH_BACK);
				success = TRUE;
			}
			if (success == FALSE && dir == SRCH_BACK) {
				/* wrap the search to end */
				clp = lback(curbp->b_linep);
				curwp->w_dotp = clp;
				curwp->w_doto =
				    llength(curwp->w_dotp);
				if (is_find(dir) != FALSE) {
					is_cpush(SRCH_MARK);
					success = TRUE;
				}
				break;
			}
			is_lpush();
			pptr = strlen(pat);
			(void)backchar(FFRAND, 1);
			if (is_find(SRCH_BACK) != FALSE)
				is_cpush(SRCH_MARK);
			else {
				(void)forwchar(FFRAND, 1);
				ttbeep();
				success = FALSE;
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('W'):
			/* add the rest of the current word to the pattern */
			clp = curwp->w_dotp;
			cbo = curwp->w_doto;
			firstc = 1;
			if (dir == SRCH_BACK) {
				/* when isearching backwards, cbo is the start of the pattern */
				cbo += pptr;
			}

			/* if the search is case insensitive, add to pattern using lowercase */
			xcase = 0;
			for (i = 0; pat[i]; i++)
				if (ISUPPER(CHARMASK(pat[i])))
					xcase = 1;

			while (cbo < llength(clp)) {
				c = lgetc(clp, cbo++);
				if ((!firstc && !isalnum(c)) || pptr == NPAT)
					break;

				firstc = 0;
				if (!xcase && ISUPPER(c))
					c = TOLOWER(c);

				pat[pptr++] = c;
				pat[pptr] = '\0';
				/* cursor only moves when isearching forwards */
				if (dir == SRCH_FORW) {
					curwp->w_doto = cbo;
					curwp->w_flag |= WFMOVE;
					update();
				}
			}
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('H'):
		case CCHR('?'):
			is_undo(&pptr, &dir);
			if (is_peek() != SRCH_ACCM)
				success = TRUE;
			is_prompt(dir, pptr < 0, success);
			break;
		case CCHR('\\'):
		case CCHR('Q'):
			c = (char)getkey(FALSE);
			goto addchar;
		case CCHR('M'):
			c = CCHR('J');
			goto addchar;
		default:
			if (ISCTRL(c)) {
				ungetkey(c);
				curwp->w_markp = clp;
				curwp->w_marko = cbo;
				ewprintf("Mark set");
				curwp->w_flag |= WFMOVE;
				return (TRUE);
			}	/* and continue */
		case CCHR('I'):
		case CCHR('J'):
	addchar:
			if (pptr == -1)
				pptr = 0;
			if (pptr == 0)
				success = TRUE;
			pat[pptr++] = c;
			if (pptr == NPAT) {
				ewprintf("Pattern too long");
				return (FALSE);
			}
			pat[pptr] = '\0';
			is_lpush();
			if (success != FALSE) {
				if (is_find(dir) != FALSE)
					is_cpush(c);
				else {
					success = FALSE;
					ttbeep();
					is_cpush(SRCH_ACCM);
				}
			} else
				is_cpush(SRCH_ACCM);
			is_prompt(dir, FALSE, success);
		}
	}
	/* NOTREACHED */
}

static void
is_cpush(int cmd)
{
	if (++cip >= NSRCH)
		cip = 0;
	cmds[cip].s_code = cmd;
}

static void
is_lpush(void)
{
	int	ctp;

	ctp = cip + 1;
	if (ctp >= NSRCH)
		ctp = 0;
	cmds[ctp].s_code = SRCH_NOPR;
	cmds[ctp].s_doto = curwp->w_doto;
	cmds[ctp].s_dotp = curwp->w_dotp;
}

static void
is_pop(void)
{
	if (cmds[cip].s_code != SRCH_NOPR) {
		curwp->w_doto = cmds[cip].s_doto;
		curwp->w_dotp = cmds[cip].s_dotp;
		curwp->w_flag |= WFMOVE;
		cmds[cip].s_code = SRCH_NOPR;
	}
	if (--cip <= 0)
		cip = NSRCH - 1;
}

static int
is_peek(void)
{
	return (cmds[cip].s_code);
}

/* this used to always return TRUE (the return value was checked) */
static void
is_undo(int *pptr, int *dir)
{
	int	redo = FALSE;

	switch (cmds[cip].s_code) {
	case SRCH_BEGIN:
	case SRCH_NOPR:
		*pptr = -1;
	case SRCH_MARK:
		break;
	case SRCH_FORW:
		*dir = SRCH_BACK;
		redo = TRUE;
		break;
	case SRCH_BACK:
		*dir = SRCH_FORW;
		redo = TRUE;
		break;
	case SRCH_ACCM:
	default:
		*pptr -= 1;
		if (*pptr < 0)
			*pptr = 0;
		pat[*pptr] = '\0';
		break;
	}
	is_pop();
	if (redo)
		is_undo(pptr, dir);
}

static int
is_find(int dir)
{
	int	 plen, odoto;
	LINE	*odotp;

	odoto = curwp->w_doto;
	odotp = curwp->w_dotp;
	plen = strlen(pat);
	if (plen != 0) {
		if (dir == SRCH_FORW) {
			(void)backchar(FFARG | FFRAND, plen);
			if (forwsrch() == FALSE) {
				curwp->w_doto = odoto;
				curwp->w_dotp = odotp;
				return (FALSE);
			}
			return (TRUE);
		}
		if (dir == SRCH_BACK) {
			(void)forwchar(FFARG | FFRAND, plen);
			if (backsrch() == FALSE) {
				curwp->w_doto = odoto;
				curwp->w_dotp = odotp;
				return (FALSE);
			}
			return (TRUE);
		}
		ewprintf("bad call to is_find");
		return (FALSE);
	}
	return (FALSE);
}

/*
 * If called with "dir" not one of SRCH_FORW or SRCH_BACK, this routine used
 * to print an error message.  It also used to return TRUE or FALSE, depending
 * on if it liked the "dir".  However, none of the callers looked at the
 * status, so I just made the checking vanish.
 */
static void
is_prompt(int dir, int flag, int success)
{
	if (dir == SRCH_FORW) {
		if (success != FALSE)
			is_dspl("I-search", flag);
		else
			is_dspl("Failing I-search", flag);
	} else if (dir == SRCH_BACK) {
		if (success != FALSE)
			is_dspl("I-search backward", flag);
		else
			is_dspl("Failing I-search backward", flag);
	} else
		ewprintf("Broken call to is_prompt");
}

/*
 * Prompt writing routine for the incremental search.  The "prompt" is just
 * a string. The "flag" determines whether pat should be printed.
 */
static void
is_dspl(char *prompt, int flag)
{
	if (flag != FALSE)
		ewprintf("%s: ", prompt);
	else
		ewprintf("%s: %s", prompt, pat);
}

/*
 * Query Replace.
 *	Replace strings selectively.  Does a search and replace operation.
 */
/* ARGSUSED */
int
queryrepl(int f, int n)
{
	int	s;
	int	rcnt = 0;		/* replacements made so far	*/
	int	plen;			/* length of found string	*/
	char	news[NPAT], *rep;	/* replacement string		*/

#ifndef NO_MACRO
	if (macrodef) {
		ewprintf("Can't query replace in macro");
		return (FALSE);
	}
#endif /* !NO_MACRO */

	if ((s = readpattern("Query replace")) != TRUE)
		return (s);
	if ((rep = eread("Query replace %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR, pat)) == NULL)
		return (ABORT);
	else if (rep[0] == '\0')
		news[0] = '\0';
	ewprintf("Query replacing %s with %s:", pat, news);
	plen = strlen(pat);

	/*
	 * Search forward repeatedly, checking each time whether to insert
	 * or not.  The "!" case makes the check always true, so it gets put
	 * into a tighter loop for efficiency.
	 */
	while (forwsrch() == TRUE) {
retry:
		update();
		switch (getkey(FALSE)) {
		case 'y':
		case ' ':
			if (lreplace((RSIZE)plen, news, f) == FALSE)
				return (FALSE);
			rcnt++;
			break;
		case '.':
			if (lreplace((RSIZE)plen, news, f) == FALSE)
				return (FALSE);
			rcnt++;
			goto stopsearch;
		/* ^G, CR or ESC */
		case CCHR('G'):
			(void)ctrlg(FFRAND, 0);
		case CCHR('['):
		case CCHR('M'):
			goto stopsearch;
		case '!':
			do {
				if (lreplace((RSIZE)plen, news, f) == FALSE)
					return (FALSE);
				rcnt++;
			} while (forwsrch() == TRUE);
			goto stopsearch;
		case 'n':
		case CCHR('H'):
		/* To not replace */
		case CCHR('?'):
			break;
		default:
			ewprintf("y/n or <SP>/<DEL>: replace/don't, [.] repl-end, [!] repl-rest, <CR>/<ESC> quit");
			goto retry;
		}
	}
stopsearch:
	curwp->w_flag |= WFHARD;
	update();
	if (rcnt == 1)
		ewprintf("Replaced 1 occurrence");
	else
		ewprintf("Replaced %d occurrences", rcnt);
	return (TRUE);
}

/*
 * Replace string globally without individual prompting.
 */
/* ARGSUSED */
int
replstr(int f, int n)
{
	char	news[NPAT];
	int	s, plen, rcnt = 0;
	char	*r;

	if ((s = readpattern("Replace string")) != TRUE)
		return s;

	r = eread("Replace string %s with: ", news, NPAT,
	    EFNUL | EFNEW | EFCR,  pat);
	if (r == NULL)
		 return (ABORT);

	plen = strlen(pat);
	while (forwsrch() == TRUE) {
		update();
		if (lreplace((RSIZE)plen, news, f) == FALSE)
			return (FALSE);

		rcnt++;
	}

	curwp->w_flag |= WFHARD;
	update();

	if (rcnt == 1)
		ewprintf("Replaced 1 occurrence");
	else
		ewprintf("Replaced %d occurrences", rcnt);

	return (TRUE);
}

/*
 * This routine does the real work of a forward search.  The pattern is sitting
 * in the external variable "pat".  If found, dot is updated, the window system
 * is notified of the change, and TRUE is returned.  If the string isn't found,
 * FALSE is returned.
 */
int
forwsrch(void)
{
	LINE	*clp, *tlp;
	int	 cbo, tbo, c, i, xcase = 0;
	char	*pp;

	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	for (i = 0; pat[i]; i++)
		if (ISUPPER(CHARMASK(pat[i])))
			xcase = 1;
	for (;;) {
		if (cbo == llength(clp)) {
			if ((clp = lforw(clp)) == curbp->b_linep)
				break;
			cbo = 0;
			c = CCHR('J');
		} else
			c = lgetc(clp, cbo++);
		if (eq(c, pat[0], xcase) != FALSE) {
			tlp = clp;
			tbo = cbo;
			pp = &pat[1];
			while (*pp != 0) {
				if (tbo == llength(tlp)) {
					tlp = lforw(tlp);
					if (tlp == curbp->b_linep)
						goto fail;
					tbo = 0;
					c = CCHR('J');
				} else
					c = lgetc(tlp, tbo++);
				if (eq(c, *pp++, xcase) == FALSE)
					goto fail;
			}
			curwp->w_dotp = tlp;
			curwp->w_doto = tbo;
			curwp->w_flag |= WFMOVE;
			return (TRUE);
		}
fail:		;
	}
	return (FALSE);
}

/*
 * This routine does the real work of a backward search.  The pattern is
 * sitting in the external variable "pat".  If found, dot is updated, the
 * window system is notified of the change, and TRUE is returned.  If the
 * string isn't found, FALSE is returned.
 */
int
backsrch(void)
{
	LINE	*clp, *tlp;
	int	 cbo, tbo, c, i, xcase = 0;
	char	*epp, *pp;

	for (epp = &pat[0]; epp[1] != 0; ++epp);
	clp = curwp->w_dotp;
	cbo = curwp->w_doto;
	for (i = 0; pat[i]; i++)
		if (ISUPPER(CHARMASK(pat[i])))
			xcase = 1;
	for (;;) {
		if (cbo == 0) {
			clp = lback(clp);
			if (clp == curbp->b_linep)
				return (FALSE);
			cbo = llength(clp) + 1;
		}
		if (--cbo == llength(clp))
			c = CCHR('J');
		else
			c = lgetc(clp, cbo);
		if (eq(c, *epp, xcase) != FALSE) {
			tlp = clp;
			tbo = cbo;
			pp = epp;
			while (pp != &pat[0]) {
				if (tbo == 0) {
					tlp = lback(tlp);
					if (tlp == curbp->b_linep)
						goto fail;
					tbo = llength(tlp) + 1;
				}
				if (--tbo == llength(tlp))
					c = CCHR('J');
				else
					c = lgetc(tlp, tbo);
				if (eq(c, *--pp, xcase) == FALSE)
					goto fail;
			}
			curwp->w_dotp = tlp;
			curwp->w_doto = tbo;
			curwp->w_flag |= WFMOVE;
			return (TRUE);
		}
fail:		;
	}
	/* NOTREACHED */
}

/*
 * Compare two characters.  The "bc" comes from the buffer.  It has its case
 * folded out. The "pc" is from the pattern.
 */
static int
eq(int bc, int pc, int xcase)
{
	bc = CHARMASK(bc);
	pc = CHARMASK(pc);
	if (bc == pc)
		return (TRUE);
	if (xcase)
		return (FALSE);
	if (ISUPPER(bc))
		return (TOLOWER(bc) == pc);
	if (ISUPPER(pc))
		return (bc == TOLOWER(pc));
	return (FALSE);
}

/*
 * Read a pattern.  Stash it in the external variable "pat".  The "pat" is not
 * updated if the user types in an empty line.  If the user typed an empty
 * line, and there is no old pattern, it is an error.  Display the old pattern,
 * in the style of Jeff Lomicka.  There is some do-it-yourself control
 * expansion.
 */
int
readpattern(char *prompt)
{
	char	tpat[NPAT], *rep;
	int	retval;

	if (pat[0] == '\0')
		rep = eread("%s: ", tpat, NPAT, EFNEW | EFCR, prompt);
	else
		rep = eread("%s: (default %s) ", tpat, NPAT,
		    EFNUL | EFNEW | EFCR, prompt, pat);

	/* specified */
	if (rep == NULL) {
		retval = ABORT;
	} else if (rep[0] != '\0') {
		(void)strlcpy(pat, tpat, sizeof(pat));
		retval = TRUE;
	} else if (pat[0] != '\0') {
		retval = TRUE;
	} else
		retval = FALSE;
	return (retval);
}
