/*	$Id: tbl_layout.c,v 1.3 2010/10/15 22:50:28 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/queue.h>
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "out.h"
#include "term.h"
#include "tbl_extern.h"

struct	tbl_phrase {
	char		 name;
	enum tbl_cellt	 key;
};

#define	KEYS_MAX	 17

static	const struct tbl_phrase keys[KEYS_MAX] = {
	{ 'c',		 TBL_CELL_CENTRE },
	{ 'C',		 TBL_CELL_CENTRE },
	{ 'r',		 TBL_CELL_RIGHT },
	{ 'R',		 TBL_CELL_RIGHT },
	{ 'l',		 TBL_CELL_LEFT },
	{ 'L',		 TBL_CELL_LEFT },
	{ 'n',		 TBL_CELL_NUMBER },
	{ 'N',		 TBL_CELL_NUMBER },
	{ 's',		 TBL_CELL_SPAN },
	{ 'S',		 TBL_CELL_SPAN },
	{ 'a',		 TBL_CELL_LONG },
	{ 'A',		 TBL_CELL_LONG },
	{ '^',		 TBL_CELL_DOWN },
	{ '-',		 TBL_CELL_HORIZ },
	{ '_',		 TBL_CELL_HORIZ },
	{ '=',		 TBL_CELL_DHORIZ },
	{ '|',		 TBL_CELL_VERT }
};

static	int		mods(struct tbl *, struct tbl_cell *, 
				const char *, int, 
				const char *, int, int);
static	int		cell(struct tbl *, struct tbl_row *, 
				const char *, int, int);
static	int		row(struct tbl *, const char *,
				int, const char *, int *);


static int
mods(struct tbl *tbl, struct tbl_cell *cp, const char *p, 
		int pp, const char *f, int ln, int pos)
{
	char		 buf[5];
	int		 i;

	/* 
	 * XXX: since, at least for now, modifiers are non-conflicting
	 * (are separable by value, regardless of position), we let
	 * modifiers come in any order.  The existing tbl doesn't let
	 * this happen.
	 */

	if (0 == p[pp])
		return(1);

	/* Parse numerical spacing from modifier string. */

	if (isdigit((u_char)p[pp])) {
		for (i = 0; i < 4; i++) {
			if ( ! isdigit((u_char)p[pp + i]))
				break;
			buf[i] = p[pp + i];
		}
		buf[i] = 0;

		/* No greater than 4 digits. */

		if (4 == i)
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, pos + pp));

		/* 
		 * We can't change the spacing in any subsequent layout
		 * definitions.  FIXME: I don't think we can change the
		 * spacing for a column at all, after it's already been
		 * initialised.
		 */

		if (TBL_PART_CLAYOUT != tbl->part)
			cp->spacing = atoi(buf);
		else if ( ! tbl_warnx(tbl, ERR_SYNTAX, f, ln, pos + pp))
			return(0);
		
		/* Continue parsing modifiers. */

		return(mods(tbl, cp, p, pp + i, f, ln, pos));
	} 

	/* TODO: GNU has many more extensions. */

	switch (p[pp]) {
	case ('z'):
		/* FALLTHROUGH */
	case ('Z'):
		cp->flags |= TBL_CELL_WIGN;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('w'):
		/* FALLTHROUGH */
	case ('W'):  /* XXX for now, ignore minimal column width */
		while (isdigit((u_char)p[++pp]));
		return(mods(tbl, cp, p, pp, f, ln, pos));
	case ('u'):
		/* FALLTHROUGH */
	case ('U'):
		cp->flags |= TBL_CELL_UP;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('e'):
		/* FALLTHROUGH */
	case ('E'):
		cp->flags |= TBL_CELL_EQUAL;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('t'):
		/* FALLTHROUGH */
	case ('T'):
		cp->flags |= TBL_CELL_TALIGN;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('d'):
		/* FALLTHROUGH */
	case ('D'):
		cp->flags |= TBL_CELL_BALIGN;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('f'):
		pp++;
		/* FALLTHROUGH */
	case ('B'):
		/* FALLTHROUGH */
	case ('I'):
		/* FALLTHROUGH */
	case ('b'):
		/* FALLTHROUGH */
	case ('i'):
		break;
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, pos + pp));
	}

	switch (p[pp]) {
	case ('b'):
		/* FALLTHROUGH */
	case ('B'):
		cp->flags |= TBL_CELL_BOLD;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	case ('i'):
		/* FALLTHROUGH */
	case ('I'):
		cp->flags |= TBL_CELL_ITALIC;
		return(mods(tbl, cp, p, pp + 1, f, ln, pos));
	default:
		break;
	}

	return(tbl_errx(tbl, ERR_SYNTAX, f, ln, pos + pp));
}


static int
cell(struct tbl *tbl, struct tbl_row *rp, 
		const char *f, int ln, int pos)
{
	struct tbl_cell	*cp;
	const char	*p;
	int		 j, i;
	enum tbl_cellt	 c;

	/* Parse the column position (`r', `R', `|', ...). */

	c = TBL_CELL_MAX;
	for (p = tbl_last(), i = 0; i < KEYS_MAX; i++) {
		if (keys[i].name != p[0])
			continue;
		c = keys[i].key;
		break;
	}

	if (i == KEYS_MAX)
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, pos));

	/* Extra check for the double-vertical. */

	if (TBL_CELL_VERT == c && '|' == p[1]) {
		j = 2;
		c = TBL_CELL_DVERT;
	} else
		j = 1;
	
	/* Disallow subsequent spacers. */

	/* LINTED */
	cp = TAILQ_LAST(&rp->cell, tbl_cellh);

	if (cp && (TBL_CELL_VERT == c || TBL_CELL_DVERT == c) && 
			(TBL_CELL_VERT == cp->pos || 
			 TBL_CELL_DVERT == cp->pos))
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, pos));

	/* Allocate cell then parse its modifiers. */

	if (NULL == (cp = tbl_cell_alloc(rp, c)))
		return(0);
	return(mods(tbl, cp, p, j, f, ln, pos));
}


static int
row(struct tbl *tbl, const char *f, int ln,
		const char *p, int *pos)
{
	struct tbl_row	*rp;
	int		 sv;

	rp = tbl_row_alloc(tbl);
again:
	sv = *pos;

	/*
	 * EBNF describing this section:
	 *
	 * row		::= row_list [:space:]* [.]?[\n]
	 * row_list	::= [:space:]* row_elem row_tail
	 * row_tail	::= [:space:]*[,] row_list |
	 *                  epsilon
	 * row_elem	::= [\t\ ]*[:alpha:]+
	 */

	switch (tbl_next(p, pos)) {
	case (TBL_TOK_TAB):
		/* FALLTHROUGH */
	case (TBL_TOK_SPACE):
		goto again;
	case (TBL_TOK_WORD):
		if ( ! cell(tbl, rp, f, ln, sv))
			return(0);
		goto again;
	case (TBL_TOK_COMMA):
		if (NULL == TAILQ_FIRST(&rp->cell))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		return(row(tbl, f, ln, p, pos));
	case (TBL_TOK_PERIOD):
		if (NULL == TAILQ_FIRST(&rp->cell))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		tbl->part = TBL_PART_DATA;
		break;
	case (TBL_TOK_NIL):
		if (NULL == TAILQ_FIRST(&rp->cell))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		break;
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
	}

	return(1);
}


int
tbl_layout(struct tbl *tbl, const char *f, int ln, const char *p)
{
	int		 pos;

	pos = 0;
	return(row(tbl, f, ln, p, &pos));
}
