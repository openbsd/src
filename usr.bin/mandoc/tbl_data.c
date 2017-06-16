/*	$OpenBSD: tbl_data.c,v 1.30 2017/06/16 20:00:41 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2015, 2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmandoc.h"
#include "libroff.h"

static	void		 getdata(struct tbl_node *, struct tbl_span *,
				int, const char *, int *);
static	struct tbl_span	*newspan(struct tbl_node *, int,
				struct tbl_row *);


static void
getdata(struct tbl_node *tbl, struct tbl_span *dp,
		int ln, const char *p, int *pos)
{
	struct tbl_dat	*dat;
	struct tbl_cell	*cp;
	int		 sv;

	/* Advance to the next layout cell, skipping spanners. */

	cp = dp->last == NULL ? dp->layout->first : dp->last->layout->next;
	while (cp != NULL && cp->pos == TBL_CELL_SPAN)
		cp = cp->next;

	/*
	 * If the current layout row is out of cells, allocate
	 * a new cell if another row of the table has at least
	 * this number of columns, or discard the input if we
	 * are beyond the last column of the table as a whole.
	 */

	if (cp == NULL) {
		if (dp->layout->last->col + 1 < dp->opts->cols) {
			cp = mandoc_calloc(1, sizeof(*cp));
			cp->pos = TBL_CELL_LEFT;
			dp->layout->last->next = cp;
			cp->col = dp->layout->last->col + 1;
			dp->layout->last = cp;
		} else {
			mandoc_msg(MANDOCERR_TBLDATA_EXTRA, tbl->parse,
			    ln, *pos, p + *pos);
			while (p[*pos])
				(*pos)++;
			return;
		}
	}

	dat = mandoc_calloc(1, sizeof(*dat));
	dat->layout = cp;
	dat->pos = TBL_DATA_NONE;
	dat->spans = 0;
	for (cp = cp->next; cp != NULL; cp = cp->next)
		if (cp->pos == TBL_CELL_SPAN)
			dat->spans++;
		else
			break;

	if (dp->last == NULL)
		dp->first = dat;
	else
		dp->last->next = dat;
	dp->last = dat;

	sv = *pos;
	while (p[*pos] && p[*pos] != tbl->opts.tab)
		(*pos)++;

	/*
	 * Check for a continued-data scope opening.  This consists of a
	 * trailing `T{' at the end of the line.  Subsequent lines,
	 * until a standalone `T}', are included in our cell.
	 */

	if (*pos - sv == 2 && p[sv] == 'T' && p[sv + 1] == '{') {
		tbl->part = TBL_PART_CDATA;
		return;
	}

	dat->string = mandoc_strndup(p + sv, *pos - sv);

	if (p[*pos])
		(*pos)++;

	if ( ! strcmp(dat->string, "_"))
		dat->pos = TBL_DATA_HORIZ;
	else if ( ! strcmp(dat->string, "="))
		dat->pos = TBL_DATA_DHORIZ;
	else if ( ! strcmp(dat->string, "\\_"))
		dat->pos = TBL_DATA_NHORIZ;
	else if ( ! strcmp(dat->string, "\\="))
		dat->pos = TBL_DATA_NDHORIZ;
	else
		dat->pos = TBL_DATA_DATA;

	if ((dat->layout->pos == TBL_CELL_HORIZ ||
	    dat->layout->pos == TBL_CELL_DHORIZ ||
	    dat->layout->pos == TBL_CELL_DOWN) &&
	    dat->pos == TBL_DATA_DATA && *dat->string != '\0')
		mandoc_msg(MANDOCERR_TBLDATA_SPAN,
		    tbl->parse, ln, sv, dat->string);
}

int
tbl_cdata(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_dat	*dat;
	size_t		 sz;

	dat = tbl->last_span->last;

	if (p[pos] == 'T' && p[pos + 1] == '}') {
		pos += 2;
		if (p[pos] == tbl->opts.tab) {
			tbl->part = TBL_PART_DATA;
			pos++;
			while (p[pos] != '\0')
				getdata(tbl, tbl->last_span, ln, p, &pos);
			return 1;
		} else if (p[pos] == '\0') {
			tbl->part = TBL_PART_DATA;
			return 1;
		}

		/* Fallthrough: T} is part of a word. */
	}

	dat->pos = TBL_DATA_DATA;
	dat->block = 1;

	if (dat->string != NULL) {
		sz = strlen(p + pos) + strlen(dat->string) + 2;
		dat->string = mandoc_realloc(dat->string, sz);
		(void)strlcat(dat->string, " ", sz);
		(void)strlcat(dat->string, p + pos, sz);
	} else
		dat->string = mandoc_strdup(p + pos);

	if (dat->layout->pos == TBL_CELL_DOWN)
		mandoc_msg(MANDOCERR_TBLDATA_SPAN, tbl->parse,
		    ln, pos, dat->string);

	return 0;
}

static struct tbl_span *
newspan(struct tbl_node *tbl, int line, struct tbl_row *rp)
{
	struct tbl_span	*dp;

	dp = mandoc_calloc(1, sizeof(*dp));
	dp->line = line;
	dp->opts = &tbl->opts;
	dp->layout = rp;
	dp->prev = tbl->last_span;

	if (dp->prev == NULL) {
		tbl->first_span = dp;
		tbl->current_span = NULL;
	} else
		dp->prev->next = dp;
	tbl->last_span = dp;

	return dp;
}

void
tbl_data(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_row	*rp;
	struct tbl_cell	*cp;
	struct tbl_span	*sp, *spi;
	struct tbl_dat	*dp;
	int		 have_data, spans;

	rp = (sp = tbl->last_span) == NULL ? tbl->first_row :
	    sp->pos == TBL_SPAN_DATA && sp->layout->next != NULL ?
	    sp->layout->next : sp->layout;

	assert(rp != NULL);

	sp = newspan(tbl, ln, rp);

	if ( ! strcmp(p, "_")) {
		sp->pos = TBL_SPAN_HORIZ;
		return;
	} else if ( ! strcmp(p, "=")) {
		sp->pos = TBL_SPAN_DHORIZ;
		return;
	}
	sp->pos = TBL_SPAN_DATA;

	while (p[pos] != '\0')
		getdata(tbl, sp, ln, p, &pos);

	/*
	 * If this span contains some data,
	 * make sure at least part of it gets printed.
	 */

	have_data = 0;
	cp = rp->first;
	for (dp = sp->first; dp != NULL; dp = dp->next) {
		if (dp->pos == TBL_DATA_DATA && *dp->string != '\0') {
			if (cp == NULL ||
			    (cp->pos != TBL_CELL_HORIZ &&
			     cp->pos != TBL_CELL_DHORIZ))
				return;
			have_data = 1;
		}
		spans = dp->spans;
		while (spans-- >= 0) {
			if (cp != NULL)
				cp = cp->next;
		}
	}
	if (have_data == 0 || rp->next == NULL)
		return;

	/*
	 * There is some data, but it would all get lost
	 * due to horizontal lines in the layout.
	 * Insert an empty span to consume the layout row.
	 */

	tbl->last_span = sp->prev;
	spi = newspan(tbl, ln, rp);
	spi->pos = TBL_SPAN_DATA;
	spi->next = sp;
	tbl->last_span = sp;
	sp->prev = spi;
	sp->layout = rp->next;
	cp = sp->layout->first;
	for (dp = sp->first; dp != NULL; dp = dp->next) {
		dp->layout = cp;
		dp->spans = 0;
		if (cp != NULL)
			cp = cp->next;
		while (cp != NULL && cp->pos == TBL_CELL_SPAN) {
			dp->spans++;
			cp = cp->next;
		}
	}
}
