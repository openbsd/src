/* $OpenBSD: cmode.c,v 1.1 2008/06/12 01:58:44 kjell Exp $ */
/*
 * This file is in the public domain.
 *
 * Author: Kjell Wooding <kjell@openbsd.org>
 */

/*
 * Implement an non-irritating KNF-compliant mode for editing
 * C code.
 */
#include <ctype.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int cc_strip_trailp = TRUE;	/* Delete Trailing space? */
static int cc_basic_indent = 8;		/* Basic Indent multiple */
static int cc_cont_indent = 4;		/* Continued line indent */
static int cc_colon_indent = -8;	/* Label / case indent */

static int getmatch(int, int);
static int getindent(const struct line *, int *);

/* Keymaps */

static PF cmode_brace[] = {
	cc_char,	/* } */
};

static PF cmode_ci[] = {
	cc_tab,		/* ^I */
	rescan,		/* ^J */
	rescan,		/* ^K */
	rescan,		/* ^L */
	cc_lfindent,	/* ^M */
};

static PF cmode_colon[] = {
	cc_char,	/* : */
	rescan,		/* ; */
};

static struct KEYMAPE (3 + IMAPEXT) cmodemap = {
	3,
	3 + IMAPEXT,
	rescan,
	{
		{ CCHR('I'), CCHR('M'), cmode_ci, NULL },
		{ ':', ';', cmode_colon, NULL },
		{ '}', '}', cmode_brace, NULL }
	}
};

/* Funtion, Mode hooks */

void
cmode_init(void)
{
	funmap_add(cmode, "c-mode");
	funmap_add(cc_char, "c-handle-special-char");
	funmap_add(cc_tab, "c-tab-or-indent");
	funmap_add(cc_indent, "c-indent");
	funmap_add(cc_lfindent, "c-indent-and-newline");
	maps_add((KEYMAP *)&cmodemap, "c-mode");
}

/*
 * Enable/toggle c-mode
 */
int
cmode(int f, int n)
{
	return(changemode(f, n, "c-mode"));
}

/*
 * Handle special C character - selfinsert then indent.
 */
int
cc_char(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (selfinsert(FFRAND, n) == FALSE)
		return (FALSE);
	return (cc_indent(FFRAND, n));
}

/*
 * If we are in the whitespace at the beginning of the line,
 * simply act as a regular tab. If we are not, indent
 * current line according to whitespace rules.
 */
int
cc_tab(int f, int n)
{
	int lo;
	int inwhitep = FALSE;	/* In leading whitespace? */
	
	for (lo = 0; lo < llength(curwp->w_dotp); lo++) {
		if (!isspace(lgetc(curwp->w_dotp, lo)))
			break;
		if (lo == curwp->w_doto)
			inwhitep = TRUE;
	}

	/* Special case: we could be at the end of a whitespace line */
	if (lo == llength(curwp->w_dotp) && curwp->w_doto == lo)
		inwhitep = TRUE;
	
	/* If empty line, or in whitespace */
	if (llength(curwp->w_dotp) == 0 || inwhitep)
		return (selfinsert(f, n));

	return (cc_indent(FFRAND, 1));
}

/*
 * Attempt to indent current line according to KNF rules.
 */
int
cc_indent(int f, int n)
{
	int pi, mi;			/* Previous indents */
	int ci, dci;			/* current indent, don't care */
	int lo;
	int c;
	int nonblankp;
	struct line *lp;

	if (n < 0)
		return (FALSE);

	if (cc_strip_trailp)
		deltrailwhite(FFRAND, 1);

	/* Search backwards for a nonblank, non preprocessor line */
	lp = curwp->w_dotp;
	nonblankp = FALSE;
	while (lback(lp) != curbp->b_headp && !nonblankp) {
		lp = lback(lp);
		for (lo = 0; lo < llength(lp); lo++) {
			if (!isspace(c = lgetc(lp, lo))) {
				/* Leading # is a blank */
				if (c != '#')	
					nonblankp = TRUE;
				break;
			}
		}
	}

	pi = getindent(lp, &mi);

	/* Strip leading space on current line */
	delleadwhite(FFRAND, 1);
	/* current indent is computed only to current position */
	dci = getindent(curwp->w_dotp, &ci);
	
	if (pi + ci < 0)
		return(indent(FFOTHARG, 0));
	else
		return(indent(FFOTHARG, pi + ci));
}

/*
 * Indent-and-newline (technically, newline then indent)
 */
int
cc_lfindent(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (newline(FFRAND, 1) == FALSE)
		return (FALSE);
	return (cc_indent(FFRAND, n));
}

/*
 * Get the level of indention after line lp is processed
 * Note getindent has two returns: curi, nexti.
 * curi = value if indenting current line.
 * return value = value affecting subsequent lines.
 * note, we only process up to offset op.
 * set to llength(lp) for the whole line.
 */
static int
getindent(const struct line *lp, int *curi)
{
	int lo, co;		/* leading space,  current offset*/
	int nicol = 0;		/* position count */
	int c = '\0';		/* current char */
	int newind = 0;		/* new index value */
	int stringp = FALSE;	/* in string? */
	int escp = FALSE;	/* Escape char? */
	int lastc = '\0';	/* Last matched string delimeter */
	int nparen = 0;		/* paren count */
	int obrace = 0;		/* open brace count */
	int cbrace = 0;		/* close brace count */
	int contp = FALSE;	/* Continue? */
	int firstseenp = FALSE;	/* First nonspace encountered? */
	int colonp = FALSE;	/* Did we see a colon? */
	int questionp = FALSE;	/* Did we see a question mark? */
	
	*curi = 0;

	/* Compute leading space */
	for (lo = 0; lo < llength(lp); lo++) {
		if (!isspace(c = lgetc(lp, lo)))
			break;
		if (c == '\t'
#ifdef NOTAB
		    && !(curbp-b_flag & BFNOTAB)
#endif /* NOTAB */
		    ) {
			nicol |= 0x07;
		}
		nicol++;
	}

	/* If last line was blank, choose 0 */
	if (lo == llength(lp))
		nicol = 0;

	if (c == '#')
		return (0);

	newind = 0;
	/* Compute modifiers */
	for (co = lo; co < llength(lp); co++) {
		c = lgetc(lp, co);
		/* We have a non-whitespace char */
		if (!firstseenp && !isspace(c)) {
			contp = TRUE;
			firstseenp = TRUE; 
		}
		if (c == '\\')
			escp = !escp;
		else if (stringp) {
			if (!escp && (c == '"' || c == '\'')) {
				/* unescaped string char */
				if (getmatch(c, lastc))
					stringp = FALSE;
			}
		} else if (c == '"' || c == '\'') {
			stringp = TRUE;
			lastc = c;
		} else if (c == '(') {
			nparen++;
		} else if (c == ')') {
			nparen--;
		} else if (c == '{') {
			obrace++;
			firstseenp = FALSE;
			contp = FALSE;
		} else if (c == '}') {
			cbrace++;
		} else if (c == '?') {
			questionp = TRUE;
		} else if (c == ':') {
			/* ignore (foo ? bar : baz) construct */
			if (!questionp)
				colonp = TRUE;
		} else if (c == ';') {
			if (nparen > 0)
				contp = FALSE;
		}

		/* Reset escape character match */
		if (c != '\\')
			escp = FALSE;
	}
	/*
	 * If not terminated with a semicolon, and brace or paren open.
	 * we continue
	 */
	if (colonp) {
		*curi += cc_colon_indent;
		newind -= cc_colon_indent;
	}

	*curi -= (cbrace) * cc_basic_indent;
	newind += obrace * cc_basic_indent;

	if (nparen < 0)
		newind -= cc_cont_indent;
	else if (nparen > 0)
		newind += cc_cont_indent;
	newind += nicol;
	*curi += nicol;

	return (newind);
}

/*
 * Given a delimeter and its purported mate, tell us if they
 * match.
 */
static int
getmatch(int c, int mc)
{
	int match = FALSE;

	switch (c) {
	case '"':
		match = (mc == '"');
		break;
	case '\'':
		match = (mc == '\'');
		break;
	case '(':
		match = (mc == ')');
		break;
	case '[':
		match = (mc == ']');
		break;
	case '{':
		match = (mc == '}');
		break;
	}

	return (match);
}
