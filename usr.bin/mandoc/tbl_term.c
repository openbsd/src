/*	$Id: tbl_term.c,v 1.1 2010/10/15 19:20:03 schwarze Exp $ */
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tbl_extern.h"

/* FIXME: `n' modifier doesn't always do the right thing. */
/* FIXME: `n' modifier doesn't use the cell-spacing buffer. */

static	void		 calc_data(struct tbl_data *);
static	void		 calc_data_literal(struct tbl_data *);
static	void		 calc_data_number(struct tbl_data *);
static	void		 calc_data_spanner(struct tbl_data *);
static	inline void	 write_char(char, int);
static	void		 write_data(const struct tbl_data *, int);
static	void		 write_data_literal(const struct tbl_data *, int);
static	void		 write_data_number(const struct tbl_data *, int);
static	void		 write_data_spanner(const struct tbl_data *, int);
static	void		 write_hframe(const struct tbl *);
static	void		 write_hrule(const struct tbl_span *);
static	void		 write_spanner(const struct tbl_head *);
static	void		 write_vframe(const struct tbl *);


int
tbl_write_term(const struct tbl *tbl)
{
	const struct tbl_span	*span;
	const struct tbl_data	*data;
	const struct tbl_head	*head;

	/*
	 * Note that the absolute widths and decimal places for headers
	 * were set when tbl_calc_term was called.
	 */

	/* First, write out our head horizontal frame. */

	write_hframe(tbl);

	/*
	 * Iterate through each span, and inside, through the global
	 * headers.  If the global header's a spanner, print it
	 * directly; if it's data, use the corresponding data in the
	 * span as the object to print.
	 */

	TAILQ_FOREACH(span, &tbl->span, entries) {
		write_vframe(tbl);

		/* Accomodate for the horizontal rule. */
		if (TBL_DATA_DHORIZ & span->flags || 
				TBL_DATA_HORIZ & span->flags) {
			write_hrule(span);
			write_vframe(tbl);
			printf("\n");
			continue;
		}

		data = TAILQ_FIRST(&span->data);
		TAILQ_FOREACH(head, &tbl->head, entries) {
			switch (head->pos) {
			case (TBL_HEAD_VERT):
				/* FALLTHROUGH */
			case (TBL_HEAD_DVERT):
				write_spanner(head);
				break;
			case (TBL_HEAD_DATA):
				write_data(data, head->width);
				if (data)
					data = TAILQ_NEXT(data, entries);
				break;
			default:
				abort();
				/* NOTREACHED */
			}
		}
		write_vframe(tbl);
		printf("\n");
	}

	/* Last, write out our tail horizontal frame. */

	write_hframe(tbl);

	return(1);
}


int
tbl_calc_term(struct tbl *tbl)
{
	struct tbl_span	*span;
	struct tbl_data	*data;
	struct tbl_head	*head;

	/* Calculate width as the max of column cells' widths. */

	TAILQ_FOREACH(span, &tbl->span, entries) {
		if (TBL_DATA_HORIZ & span->flags)
			continue;
		if (TBL_DATA_DHORIZ & span->flags)
			continue;
		if (TBL_DATA_NHORIZ & span->flags)
			continue;
		if (TBL_DATA_NDHORIZ & span->flags)
			continue;
		TAILQ_FOREACH(data, &span->data, entries)
			calc_data(data);
	}

	/* Calculate width as the simple spanner value. */

	TAILQ_FOREACH(head, &tbl->head, entries) 
		switch (head->pos) {
		case (TBL_HEAD_VERT):
			head->width = 1;
			break;
		case (TBL_HEAD_DVERT):
			head->width = 2;
			break;
		default:
			break;
		}

	return(1);
}


static void
write_hrule(const struct tbl_span *span)
{
	const struct tbl_head	*head;
	char			 c;

	/*
	 * An hrule extends across the entire table and is demarked by a
	 * standalone `_' or whatnot in lieu of a table row.  Spanning
	 * headers are marked by a `+', as are table boundaries.
	 */

	c = '-';
	if (TBL_SPAN_DHORIZ & span->flags)
		c = '=';

	/* FIXME: don't use `+' between data and a spanner! */

	TAILQ_FOREACH(head, &span->tbl->head, entries) {
		switch (head->pos) {
		case (TBL_HEAD_DATA):
			write_char(c, head->width);
			break;
		case (TBL_HEAD_DVERT):
			write_char('+', head->width);
			/* FALLTHROUGH */
		case (TBL_HEAD_VERT):
			write_char('+', head->width);
			break;
		default:
			abort();
			/* NOTREACHED */
		}
	}
}


static void
write_hframe(const struct tbl *tbl)
{
	const struct tbl_head	*head;

	if ( ! (TBL_OPT_BOX & tbl->opts || TBL_OPT_DBOX & tbl->opts))
		return;

	/* 
	 * Print out the horizontal part of a frame or double frame.  A
	 * double frame has an unbroken `-' outer line the width of the
	 * table, bordered by `+'.  The frame (or inner frame, in the
	 * case of the double frame) is a `-' bordered by `+' and broken
	 * by `+' whenever a span is encountered.
	 */

	if (TBL_OPT_DBOX & tbl->opts) {
		printf("+");
		TAILQ_FOREACH(head, &tbl->head, entries)
			write_char('-', head->width);
		printf("+\n");
	}

	printf("+");
	TAILQ_FOREACH(head, &tbl->head, entries) {
		switch (head->pos) {
		case (TBL_HEAD_DATA):
			write_char('-', head->width);
			break;
		default:
			write_char('+', head->width);
			break;
		}
	}
	printf("+\n");
}


static void
write_vframe(const struct tbl *tbl)
{
	/* Always just a single vertical line. */

	if ( ! (TBL_OPT_BOX & tbl->opts || TBL_OPT_DBOX & tbl->opts))
		return;
	printf("|");
}


static void
calc_data_spanner(struct tbl_data *data)
{

	/* N.B., these are horiz spanners (not vert) so always 1. */
	data->cell->head->width = 1;
}


static void
calc_data_number(struct tbl_data *data)
{
	int 		 sz, d;
	char		*dp, pnt;

	/*
	 * First calculate number width and decimal place (last + 1 for
	 * no-decimal numbers).  If the stored decimal is subsequent
	 * ours, make our size longer by that difference
	 * (right-"shifting"); similarly, if ours is subsequent the
	 * stored, then extend the stored size by the difference.
	 * Finally, re-assign the stored values.
	 */

	/* TODO: use spacing modifier. */

	assert(data->string);
	sz = (int)strlen(data->string);
	pnt = data->span->tbl->decimal;

	if (NULL == (dp = strchr(data->string, pnt)))
		d = sz + 1;
	else
		d = (int)(dp - data->string) + 1;

	sz += 2;

	if (data->cell->head->decimal > d) {
		sz += data->cell->head->decimal - d;
		d = data->cell->head->decimal;
	} else
		data->cell->head->width += 
			d - data->cell->head->decimal;

	if (sz > data->cell->head->width)
		data->cell->head->width = sz;
	if (d > data->cell->head->decimal)
		data->cell->head->decimal = d;
}


static void
calc_data_literal(struct tbl_data *data)
{
	int		 sz, bufsz;

	/* 
	 * Calculate our width and use the spacing, with a minimum
	 * spacing dictated by position (centre, e.g,. gets a space on
	 * either side, while right/left get a single adjacent space).
	 */

	assert(data->string);
	sz = (int)strlen(data->string);

	switch (data->cell->pos) {
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		bufsz = 2;
		break;
	default:
		bufsz = 1;
		break;
	}

	if (data->cell->spacing)
		bufsz = bufsz > data->cell->spacing ? 
			bufsz : data->cell->spacing;

	sz += bufsz;
	if (data->cell->head->width < sz)
		data->cell->head->width = sz;
}


static void
calc_data(struct tbl_data *data)
{

	switch (data->cell->pos) {
	case (TBL_CELL_HORIZ):
		/* FALLTHROUGH */
	case (TBL_CELL_DHORIZ):
		calc_data_spanner(data);
		break;
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		/* FALLTHROUGH */
	case (TBL_CELL_LEFT):
		/* FALLTHROUGH */
	case (TBL_CELL_RIGHT):
		calc_data_literal(data);
		break;
	case (TBL_CELL_NUMBER):
		calc_data_number(data);
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}


static void
write_data_spanner(const struct tbl_data *data, int width)
{

	/*
	 * Write spanners dictated by both our cell designation (in the
	 * layout) or as data.
	 */
	if (TBL_DATA_HORIZ & data->flags)
		write_char('-', width);
	else if (TBL_DATA_DHORIZ & data->flags)
		write_char('=', width);
	else if (TBL_CELL_HORIZ == data->cell->pos)
		write_char('-', width);
	else if (TBL_CELL_DHORIZ == data->cell->pos)
		write_char('=', width);
}


static void
write_data_number(const struct tbl_data *data, int width)
{
	char		*dp, pnt;
	int		 d, padl, sz;

	/*
	 * See calc_data_number().  Left-pad by taking the offset of our
	 * and the maximum decimal; right-pad by the remaining amount.
	 */

	sz = (int)strlen(data->string);
	pnt = data->span->tbl->decimal;

	if (NULL == (dp = strchr(data->string, pnt))) {
		d = sz + 1;
	} else {
		d = (int)(dp - data->string) + 1;
	}

	assert(d <= data->cell->head->decimal);
	assert(sz - d <= data->cell->head->width -
			data->cell->head->decimal);

	padl = data->cell->head->decimal - d + 1;
	assert(width - sz - padl);

	write_char(' ', padl);
	(void)printf("%s", data->string);
	write_char(' ', width - sz - padl);
}


static void
write_data_literal(const struct tbl_data *data, int width)
{
	int		 padl, padr;

	padl = padr = 0;

	switch (data->cell->pos) {
	case (TBL_CELL_LONG):
		padl = 1;
		padr = width - (int)strlen(data->string) - 1;
		break;
	case (TBL_CELL_CENTRE):
		padl = width - (int)strlen(data->string);
		if (padl % 2)
			padr++;
		padl /= 2;
		padr += padl;
		break;
	case (TBL_CELL_RIGHT):
		padl = width - (int)strlen(data->string);
		break;
	default:
		padr = width - (int)strlen(data->string);
		break;
	}

	write_char(' ', padl);
	(void)printf("%s", data->string);
	write_char(' ', padr);
}


static void
write_data(const struct tbl_data *data, int width)
{

	if (NULL == data) {
		write_char(' ', width);
		return;
	}

	if (TBL_DATA_HORIZ & data->flags || 
			TBL_DATA_DHORIZ & data->flags) {
		write_data_spanner(data, width);
		return;
	}

	switch (data->cell->pos) {
	case (TBL_CELL_HORIZ):
		/* FALLTHROUGH */
	case (TBL_CELL_DHORIZ):
		write_data_spanner(data, width);
		break;
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		/* FALLTHROUGH */
	case (TBL_CELL_LEFT):
		/* FALLTHROUGH */
	case (TBL_CELL_RIGHT):
		write_data_literal(data, width);
		break;
	case (TBL_CELL_NUMBER):
		write_data_number(data, width);
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}


static void
write_spanner(const struct tbl_head *head)
{
	char		*p;

	p = NULL;
	switch (head->pos) {
	case (TBL_HEAD_VERT):
		p = "|";
		break;
	case (TBL_HEAD_DVERT):
		p = "||";
		break;
	default:
		break;
	}

	assert(p);
	printf("%s", p);
}


static inline void
write_char(char c, int len)
{
	int		 i;

	for (i = 0; i < len; i++)
		printf("%c", c);
}
