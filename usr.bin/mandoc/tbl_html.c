/*	$Id: tbl_html.c,v 1.3 2011/01/16 01:11:50 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@kth.se>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "html.h"

static	size_t	 html_tbl_len(size_t, void *);
static	size_t	 html_tbl_strlen(const char *, void *);

/* ARGSUSED */
static size_t
html_tbl_len(size_t sz, void *arg)
{
	
	return(sz);
}

/* ARGSUSED */
static size_t
html_tbl_strlen(const char *p, void *arg)
{

	return(strlen(p));
}

void
print_tbl(struct html *h, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	const struct tbl_dat *dp;
	struct tag	*tt;
	struct htmlpair	 tag;
	struct roffsu	 su;
	struct roffcol	*col;

	/* Inhibit printing of spaces: we do padding ourselves. */

	h->flags |= HTML_NONOSPACE;
	h->flags |= HTML_NOSPACE;

	/* First pass: calculate widths. */

	if (TBL_SPAN_FIRST & sp->flags) {
		h->tbl.len = html_tbl_len;
		h->tbl.slen = html_tbl_strlen;
		tblcalc(&h->tbl, sp);
	}

	switch (sp->pos) {
	case (TBL_SPAN_HORIZ):
		/* FALLTHROUGH */
	case (TBL_SPAN_DHORIZ):
		break;
	default:
		PAIR_CLASS_INIT(&tag, "tbl");
		print_otag(h, TAG_TABLE, 1, &tag);
		print_otag(h, TAG_TR, 0, NULL);

		/* Iterate over template headers. */

		dp = sp->first;
		for (hp = sp->head; hp; hp = hp->next) {
			switch (hp->pos) {
			case (TBL_HEAD_VERT):
				/* FALLTHROUGH */
			case (TBL_HEAD_DVERT):
				continue;
			case (TBL_HEAD_DATA):
				break;
			}

			/*
			 * For the time being, use the simplest possible
			 * table styling: setting the widths of data
			 * columns.
			 */

			col = &h->tbl.cols[hp->ident];
			SCALE_HS_INIT(&su, col->width);
			bufcat_su(h, "width", &su);
			PAIR_STYLE_INIT(&tag, h);
			tt = print_otag(h, TAG_TD, 1, &tag);

			if (dp) {
				switch (dp->layout->pos) {
				case (TBL_CELL_DOWN):
					break;
				default:
					if (NULL == dp->string)
						break;
					print_text(h, dp->string);
					break;
				}
				dp = dp->next;
			}

			print_tagq(h, tt);
		}
		break;
	}

	h->flags &= ~HTML_NONOSPACE;

	/* Close out column specifiers on the last span. */

	if (TBL_SPAN_LAST & sp->flags) {
		assert(h->tbl.cols);
		free(h->tbl.cols);
		h->tbl.cols = NULL;
	}
}
