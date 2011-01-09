/*	$Id: tbl_term.c,v 1.6 2011/01/09 14:30:48 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2011 Kristaps Dzonsons <kristaps@kth.se>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"

static	size_t	term_tbl_len(size_t, void *);
static	size_t	term_tbl_strlen(const char *, void *);
static	void	tbl_char(struct termp *, char, size_t);
static	void	tbl_data(struct termp *, const struct tbl *,
			const struct tbl_dat *, 
			const struct roffcol *);
static	void	tbl_hframe(struct termp *, const struct tbl_span *);
static	void	tbl_literal(struct termp *, const struct tbl_dat *, 
			const struct roffcol *);
static	void	tbl_number(struct termp *, const struct tbl *, 
			const struct tbl_dat *, 
			const struct roffcol *);
static	void	tbl_hrule(struct termp *, const struct tbl_span *);
static	void	tbl_vframe(struct termp *, const struct tbl *);
static	void	tbl_vrule(struct termp *, const struct tbl_head *);


static size_t
term_tbl_strlen(const char *p, void *arg)
{

	return(term_strlen((const struct termp *)arg, p));
}

static size_t
term_tbl_len(size_t sz, void *arg)
{

	return(term_len((const struct termp *)arg, sz));
}

void
term_tbl(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_head	*hp;
	const struct tbl_dat	*dp;
	struct roffcol		*col;
	size_t		   	 rmargin, maxrmargin;

	rmargin = tp->rmargin;
	maxrmargin = tp->maxrmargin;

	tp->rmargin = tp->maxrmargin = TERM_MAXMARGIN;

	/* Inhibit printing of spaces: we do padding ourselves. */

	tp->flags |= TERMP_NONOSPACE;
	tp->flags |= TERMP_NOSPACE;

	/*
	 * The first time we're invoked for a given table block,
	 * calculate the table widths and decimal positions.
	 */

	if (TBL_SPAN_FIRST & sp->flags) {
		term_flushln(tp);

		tp->tbl.len = term_tbl_len;
		tp->tbl.slen = term_tbl_strlen;
		tp->tbl.arg = tp;

		tblcalc(&tp->tbl, sp);
	}

	/* Horizontal frame at the start of boxed tables. */

	if (TBL_SPAN_FIRST & sp->flags)
		tbl_hframe(tp, sp);

	/* Vertical frame at the start of each row. */

	tbl_vframe(tp, sp->tbl);

	/*
	 * Now print the actual data itself depending on the span type.
	 * Spanner spans get a horizontal rule; data spanners have their
	 * data printed by matching data to header.
	 */

	switch (sp->pos) {
	case (TBL_SPAN_HORIZ):
		/* FALLTHROUGH */
	case (TBL_SPAN_DHORIZ):
		tbl_hrule(tp, sp);
		break;
	case (TBL_SPAN_DATA):
		/* Iterate over template headers. */
		dp = sp->first;
		for (hp = sp->head; hp; hp = hp->next) {
			switch (hp->pos) {
			case (TBL_HEAD_VERT):
				/* FALLTHROUGH */
			case (TBL_HEAD_DVERT):
				tbl_vrule(tp, hp);
				continue;
			case (TBL_HEAD_DATA):
				break;
			}

			col = &tp->tbl.cols[hp->ident];
			tbl_data(tp, sp->tbl, dp, col);

			/* Go to the next data cell. */
			if (dp)
				dp = dp->next;
		}
		break;
	}

	tbl_vframe(tp, sp->tbl);
	term_flushln(tp);

	/*
	 * If we're the last row, clean up after ourselves: clear the
	 * existing table configuration and set it to NULL.
	 */

	if (TBL_SPAN_LAST & sp->flags) {
		tbl_hframe(tp, sp);
		assert(tp->tbl.cols);
		free(tp->tbl.cols);
		tp->tbl.cols = NULL;
	}

	tp->flags &= ~TERMP_NONOSPACE;
	tp->rmargin = rmargin;
	tp->maxrmargin = maxrmargin;

}

static void
tbl_hrule(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	char		 c;
	size_t		 width;

	/*
	 * An hrule extends across the entire table and is demarked by a
	 * standalone `_' or whatnot in lieu of a table row.  Spanning
	 * headers are marked by a `+', as are table boundaries.
	 */

	c = '-';
	if (TBL_SPAN_DHORIZ == sp->pos)
		c = '=';

	/* FIXME: don't use `+' between data and a spanner! */

	for (hp = sp->head; hp; hp = hp->next) {
		width = tp->tbl.cols[hp->ident].width;
		switch (hp->pos) {
		case (TBL_HEAD_DATA):
			tbl_char(tp, c, width);
			break;
		case (TBL_HEAD_DVERT):
			tbl_char(tp, '+', width);
			/* FALLTHROUGH */
		case (TBL_HEAD_VERT):
			tbl_char(tp, '+', width);
			break;
		default:
			abort();
			/* NOTREACHED */
		}
	}
}

static void
tbl_hframe(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	size_t		 width;

	if ( ! (TBL_OPT_BOX & sp->tbl->opts || 
			TBL_OPT_DBOX & sp->tbl->opts))
		return;

	/* 
	 * Print out the horizontal part of a frame or double frame.  A
	 * double frame has an unbroken `-' outer line the width of the
	 * table, bordered by `+'.  The frame (or inner frame, in the
	 * case of the double frame) is a `-' bordered by `+' and broken
	 * by `+' whenever a span is encountered.
	 */

	if (TBL_OPT_DBOX & sp->tbl->opts) {
		term_word(tp, "+");
		for (hp = sp->head; hp; hp = hp->next) {
			width = tp->tbl.cols[hp->ident].width;
			tbl_char(tp, '-', width);
		}
		term_word(tp, "+");
		term_flushln(tp);
	}

	term_word(tp, "+");
	for (hp = sp->head; hp; hp = hp->next) {
		width = tp->tbl.cols[hp->ident].width;
		switch (hp->pos) {
		case (TBL_HEAD_DATA):
			tbl_char(tp, '-', width);
			break;
		default:
			tbl_char(tp, '+', width);
			break;
		}
	}
	term_word(tp, "+");
	term_flushln(tp);
}

static void
tbl_data(struct termp *tp, const struct tbl *tbl,
		const struct tbl_dat *dp, 
		const struct roffcol *col)
{
	enum tbl_cellt	 pos;

	if (NULL == dp) {
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	}

	switch (dp->pos) {
	case (TBL_DATA_NONE):
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	case (TBL_DATA_HORIZ):
		/* FALLTHROUGH */
	case (TBL_DATA_NHORIZ):
		tbl_char(tp, '-', col->width);
		return;
	case (TBL_DATA_NDHORIZ):
		/* FALLTHROUGH */
	case (TBL_DATA_DHORIZ):
		tbl_char(tp, '=', col->width);
		return;
	default:
		break;
	}
	
	pos = dp && dp->layout ? dp->layout->pos : TBL_CELL_LEFT;

	switch (pos) {
	case (TBL_CELL_HORIZ):
		tbl_char(tp, '-', col->width);
		break;
	case (TBL_CELL_DHORIZ):
		tbl_char(tp, '=', col->width);
		break;
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		/* FALLTHROUGH */
	case (TBL_CELL_LEFT):
		/* FALLTHROUGH */
	case (TBL_CELL_RIGHT):
		tbl_literal(tp, dp, col);
		break;
	case (TBL_CELL_NUMBER):
		tbl_number(tp, tbl, dp, col);
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}

static void
tbl_vrule(struct termp *tp, const struct tbl_head *hp)
{

	switch (hp->pos) {
	case (TBL_HEAD_VERT):
		term_word(tp, "|");
		break;
	case (TBL_HEAD_DVERT):
		term_word(tp, "||");
		break;
	default:
		break;
	}
}

static void
tbl_vframe(struct termp *tp, const struct tbl *tbl)
{

	if (TBL_OPT_BOX & tbl->opts || TBL_OPT_DBOX & tbl->opts)
		term_word(tp, "|");
}

static void
tbl_char(struct termp *tp, char c, size_t len)
{
	size_t		i, sz;
	char		cp[2];

	cp[0] = c;
	cp[1] = '\0';

	sz = term_strlen(tp, cp);

	for (i = 0; i < len; i += sz)
		term_word(tp, cp);
}

static void
tbl_literal(struct termp *tp, const struct tbl_dat *dp, 
		const struct roffcol *col)
{
	size_t		 padl, padr, ssz;
	enum tbl_cellt	 pos;
	const char	*str;

	padl = padr = 0;

	pos = dp && dp->layout ? dp->layout->pos : TBL_CELL_LEFT;
	str = dp && dp->string ? dp->string : "";

	ssz = term_len(tp, 1);

	switch (pos) {
	case (TBL_CELL_LONG):
		padl = ssz;
		padr = col->width - term_strlen(tp, str) - ssz;
		break;
	case (TBL_CELL_CENTRE):
		padl = col->width - term_strlen(tp, str);
		if (padl % 2)
			padr++;
		padl /= 2;
		padr += padl;
		break;
	case (TBL_CELL_RIGHT):
		padl = col->width - term_strlen(tp, str);
		break;
	default:
		padr = col->width - term_strlen(tp, str);
		break;
	}

	tbl_char(tp, ASCII_NBRSP, padl);
	term_word(tp, str);
	tbl_char(tp, ASCII_NBRSP, padr);
}

static void
tbl_number(struct termp *tp, const struct tbl *tbl,
		const struct tbl_dat *dp,
		const struct roffcol *col)
{
	char		*cp;
	char		 buf[2];
	const char	*str;
	size_t		 sz, psz, ssz, d, padl;
	int		 i;

	/*
	 * See calc_data_number().  Left-pad by taking the offset of our
	 * and the maximum decimal; right-pad by the remaining amount.
	 */

	str = dp && dp->string ? dp->string : "";

	sz = term_strlen(tp, str);

	buf[0] = tbl->decimal;
	buf[1] = '\0';

	psz = term_strlen(tp, buf);

	if (NULL != (cp = strrchr(str, tbl->decimal))) {
		buf[1] = '\0';
		for (ssz = 0, i = 0; cp != &str[i]; i++) {
			buf[0] = str[i];
			ssz += term_strlen(tp, buf);
		}
		d = ssz + psz;
	} else
		d = sz + psz;

	sz += term_len(tp, 2);
	d += term_len(tp, 1);

	padl = col->decimal - d;

	tbl_char(tp, ASCII_NBRSP, padl);
	term_word(tp, str);
	tbl_char(tp, ASCII_NBRSP, col->width - sz - padl);
}

