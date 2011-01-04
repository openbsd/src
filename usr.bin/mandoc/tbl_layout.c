/*	$Id: tbl_layout.c,v 1.5 2011/01/04 23:44:20 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "libroff.h"

struct	tbl_phrase {
	char		 name;
	enum tbl_cellt	 key;
};

#define	KEYS_MAX	 11

static	const struct tbl_phrase keys[KEYS_MAX] = {
	{ 'c',		 TBL_CELL_CENTRE },
	{ 'r',		 TBL_CELL_RIGHT },
	{ 'l',		 TBL_CELL_LEFT },
	{ 'n',		 TBL_CELL_NUMBER },
	{ 's',		 TBL_CELL_SPAN },
	{ 'a',		 TBL_CELL_LONG },
	{ '^',		 TBL_CELL_DOWN },
	{ '-',		 TBL_CELL_HORIZ },
	{ '_',		 TBL_CELL_HORIZ },
	{ '=',		 TBL_CELL_DHORIZ },
	{ '|',		 TBL_CELL_VERT }
};

static	int		 mods(struct tbl_node *, struct tbl_cell *, 
				int, const char *, int *);
static	int		 cell(struct tbl_node *, struct tbl_row *, 
				int, const char *, int *);
static	void		 row(struct tbl_node *, int, const char *, int *);
static	struct tbl_cell *cell_alloc(struct tbl_node *, 
				struct tbl_row *, enum tbl_cellt);
static	void		 head_adjust(const struct tbl_cell *, 
				struct tbl_head *);

static int
mods(struct tbl_node *tbl, struct tbl_cell *cp, 
		int ln, const char *p, int *pos)
{
	char		 buf[5];
	int		 i;

mod:
	/* 
	 * XXX: since, at least for now, modifiers are non-conflicting
	 * (are separable by value, regardless of position), we let
	 * modifiers come in any order.  The existing tbl doesn't let
	 * this happen.
	 */
	switch (p[*pos]) {
	case ('\0'):
		/* FALLTHROUGH */
	case (' '):
		/* FALLTHROUGH */
	case ('\t'):
		/* FALLTHROUGH */
	case (','):
		/* FALLTHROUGH */
	case ('.'):
		return(1);
	default:
		break;
	}

	/* Parse numerical spacing from modifier string. */

	if (isdigit((unsigned char)p[*pos])) {
		for (i = 0; i < 4; i++) {
			if ( ! isdigit((unsigned char)p[*pos + i]))
				break;
			buf[i] = p[*pos + i];
		}
		buf[i] = '\0';

		/* No greater than 4 digits. */

		if (4 == i) {
			TBL_MSG(tbl, MANDOCERR_TBLLAYOUT, ln, *pos);
			return(0);
		}

		*pos += i;
		cp->spacing = atoi(buf);

		goto mod;
		/* NOTREACHED */
	} 

	/* TODO: GNU has many more extensions. */

	switch (tolower(p[(*pos)++])) {
	case ('z'):
		cp->flags |= TBL_CELL_WIGN;
		goto mod;
	case ('u'):
		cp->flags |= TBL_CELL_UP;
		goto mod;
	case ('e'):
		cp->flags |= TBL_CELL_EQUAL;
		goto mod;
	case ('t'):
		cp->flags |= TBL_CELL_TALIGN;
		goto mod;
	case ('d'):
		cp->flags |= TBL_CELL_BALIGN;
		goto mod;
	case ('w'):  /* XXX for now, ignore minimal column width */
		goto mod;
	case ('f'):
		break;
	case ('b'):
		/* FALLTHROUGH */
	case ('i'):
		(*pos)--;
		break;
	default:
		TBL_MSG(tbl, MANDOCERR_TBLLAYOUT, ln, *pos - 1);
		return(0);
	}

	switch (tolower(p[(*pos)++])) {
	case ('b'):
		cp->flags |= TBL_CELL_BOLD;
		goto mod;
	case ('i'):
		cp->flags |= TBL_CELL_ITALIC;
		goto mod;
	default:
		break;
	}

	TBL_MSG(tbl, MANDOCERR_TBLLAYOUT, ln, *pos - 1);
	return(0);
}

static int
cell(struct tbl_node *tbl, struct tbl_row *rp, 
		int ln, const char *p, int *pos)
{
	int		 i;
	enum tbl_cellt	 c;

	/* Parse the column position (`r', `R', `|', ...). */

	for (i = 0; i < KEYS_MAX; i++)
		if (tolower(p[*pos]) == keys[i].name)
			break;

	if (KEYS_MAX == i) {
		TBL_MSG(tbl, MANDOCERR_TBLLAYOUT, ln, *pos);
		return(0);
	}

	(*pos)++;
	c = keys[i].key;

	/* Extra check for the double-vertical. */

	if (TBL_CELL_VERT == c && '|' == p[*pos]) {
		(*pos)++;
		c = TBL_CELL_DVERT;
	} 
	
	/* Disallow adjacent spacers. */

	if (rp->last && (TBL_CELL_VERT == c || TBL_CELL_DVERT == c) &&
			(TBL_CELL_VERT == rp->last->pos || 
			 TBL_CELL_DVERT == rp->last->pos)) {
		TBL_MSG(tbl, MANDOCERR_TBLLAYOUT, ln, *pos - 1);
		return(0);
	}

	/* Allocate cell then parse its modifiers. */

	return(mods(tbl, cell_alloc(tbl, rp, c), ln, p, pos));
}


static void
row(struct tbl_node *tbl, int ln, const char *p, int *pos)
{
	struct tbl_row	*rp;

row:	/*
	 * EBNF describing this section:
	 *
	 * row		::= row_list [:space:]* [.]?[\n]
	 * row_list	::= [:space:]* row_elem row_tail
	 * row_tail	::= [:space:]*[,] row_list |
	 *                  epsilon
	 * row_elem	::= [\t\ ]*[:alpha:]+
	 */

	rp = mandoc_calloc(1, sizeof(struct tbl_row));
	if (tbl->last_row) {
		tbl->last_row->next = rp;
		tbl->last_row = rp;
	} else
		tbl->last_row = tbl->first_row = rp;

cell:
	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* Safely exit layout context. */

	if ('.' == p[*pos]) {
		tbl->part = TBL_PART_DATA;
		if (NULL == tbl->first_row) 
			TBL_MSG(tbl, MANDOCERR_TBLNOLAYOUT, ln, *pos);
		(*pos)++;
		return;
	}

	/* End (and possibly restart) a row. */

	if (',' == p[*pos]) {
		(*pos)++;
		goto row;
	} else if ('\0' == p[*pos])
		return;

	if ( ! cell(tbl, rp, ln, p, pos))
		return;

	goto cell;
	/* NOTREACHED */
}

int
tbl_layout(struct tbl_node *tbl, int ln, const char *p)
{
	int		 pos;

	pos = 0;
	row(tbl, ln, p, &pos);

	/* Always succeed. */
	return(1);
}

static struct tbl_cell *
cell_alloc(struct tbl_node *tbl, struct tbl_row *rp, enum tbl_cellt pos)
{
	struct tbl_cell	*p, *pp;
	struct tbl_head	*h, *hp;

	p = mandoc_calloc(1, sizeof(struct tbl_cell));

	if (NULL != (pp = rp->last)) {
		rp->last->next = p;
		rp->last = p;
	} else
		rp->last = rp->first = p;

	p->pos = pos;

	/*
	 * This is a little bit complicated.  Here we determine the
	 * header the corresponds to a cell.  We add headers dynamically
	 * when need be or re-use them, otherwise.  As an example, given
	 * the following:
	 *
	 * 	1  c || l 
	 * 	2  | c | l
	 * 	3  l l
	 * 	3  || c | l |.
	 *
	 * We first add the new headers (as there are none) in (1); then
	 * in (2) we insert the first spanner (as it doesn't match up
	 * with the header); then we re-use the prior data headers,
	 * skipping over the spanners; then we re-use everything and add
	 * a last spanner.  Note that VERT headers are made into DVERT
	 * ones.
	 */

	h = pp ? pp->head->next : tbl->first_head;

	if (h) {
		/* Re-use data header. */
		if (TBL_HEAD_DATA == h->pos && 
				(TBL_CELL_VERT != p->pos &&
				 TBL_CELL_DVERT != p->pos)) {
			p->head = h;
			return(p);
		}

		/* Re-use spanner header. */
		if (TBL_HEAD_DATA != h->pos && 
				(TBL_CELL_VERT == p->pos ||
				 TBL_CELL_DVERT == p->pos)) {
			head_adjust(p, h);
			p->head = h;
			return(p);
		}

		/* Right-shift headers with a new spanner. */
		if (TBL_HEAD_DATA == h->pos && 
				(TBL_CELL_VERT == p->pos ||
				 TBL_CELL_DVERT == p->pos)) {
			hp = mandoc_calloc(1, sizeof(struct tbl_head));
			hp->ident = tbl->opts.cols++;
			hp->prev = h->prev;
			if (h->prev)
				h->prev->next = hp;
			if (h == tbl->first_head)
				tbl->first_head = hp;
			h->prev = hp;
			hp->next = h;
			head_adjust(p, hp);
			p->head = hp;
			return(p);
		}

		if (NULL != (h = h->next)) {
			head_adjust(p, h);
			p->head = h;
			return(p);
		}

		/* Fall through to default case... */
	}

	hp = mandoc_calloc(1, sizeof(struct tbl_head));
	hp->ident = tbl->opts.cols++;

	if (tbl->last_head) {
		hp->prev = tbl->last_head;
		tbl->last_head->next = hp;
		tbl->last_head = hp;
	} else
		tbl->last_head = tbl->first_head = hp;

	head_adjust(p, hp);
	p->head = hp;
	return(p);
}

static void
head_adjust(const struct tbl_cell *cell, struct tbl_head *head)
{
	if (TBL_CELL_VERT != cell->pos &&
			TBL_CELL_DVERT != cell->pos) {
		head->pos = TBL_HEAD_DATA;
		return;
	}

	if (TBL_CELL_VERT == cell->pos)
		if (TBL_HEAD_DVERT != head->pos)
			head->pos = TBL_HEAD_VERT;

	if (TBL_CELL_DVERT == cell->pos)
		head->pos = TBL_HEAD_DVERT;
}

