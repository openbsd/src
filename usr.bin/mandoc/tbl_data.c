/*	$Id: tbl_data.c,v 1.2 2010/10/15 21:33:47 schwarze Exp $ */
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

/* FIXME: warn about losing data contents if cell is HORIZ. */

static	int		data(struct tbl *, struct tbl_span *, 
				const char *, int, int, 
				const char *, int, int);


int
data(struct tbl *tbl, struct tbl_span *dp, 
		const char *f, int ln, int pos, 
		const char *p, int start, int end)
{
	struct tbl_data	*dat;

	if (NULL == (dat = tbl_data_alloc(dp)))
		return(0);

	if (NULL == dat->cell)
		if ( ! tbl_warnx(tbl, ERR_SYNTAX, f, ln, pos))
			return(0);

	assert(end >= start);
	if (NULL == (dat->string = malloc((size_t)(end - start + 1))))
		return(tbl_err(tbl));

	(void)memcpy(dat->string, &p[start], (size_t)(end - start));
	dat->string[end - start] = 0;

	/* XXX: do the strcmps, then malloc(). */

	if ( ! strcmp(dat->string, "_"))
		dat->flags |= TBL_DATA_HORIZ;
	else if ( ! strcmp(dat->string, "="))
		dat->flags |= TBL_DATA_DHORIZ;
	else if ( ! strcmp(dat->string, "\\_"))
		dat->flags |= TBL_DATA_NHORIZ;
	else if ( ! strcmp(dat->string, "\\="))
		dat->flags |= TBL_DATA_NDHORIZ;
	else
		return(1);

	free(dat->string);
	dat->string = NULL;
	return(1);
}


int
tbl_data(struct tbl *tbl, const char *f, int ln, const char *p)
{
	struct tbl_span	*dp;
	int		 i, j;

	if (0 == p[0])
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, 0));

	if ('.' == p[0] && ! isdigit((u_char)p[1])) {
		/*
		 * XXX: departs from tbl convention in that we disallow
		 * macros in the data body.
		 */
		if (strncasecmp(p, ".T&", 3)) 
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, 0));
		return(tbl_data_close(tbl, f, ln));
	}

	if (NULL == (dp = tbl_span_alloc(tbl)))
		return(0);

	if ( ! strcmp(p, "_")) {
		dp->flags |= TBL_SPAN_HORIZ;
		return(1);
	} else if ( ! strcmp(p, "=")) {
		dp->flags |= TBL_SPAN_DHORIZ;
		return(1);
	}

	for (j = i = 0; p[i]; i++) {
		if (p[i] != tbl->tab)
			continue;
		if ( ! data(tbl, dp, f, ln, i, p, j, i))
			return(0);
		j = i + 1;
	}

	return(data(tbl, dp, f, ln, i, p, j, i));
}


int
tbl_data_close(struct tbl *tbl, const char *f, int ln)
{
	struct tbl_span	*span;

	/* LINTED */
	span = TAILQ_LAST(&tbl->span, tbl_spanh);
	if (NULL == span)
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, 0));
	if (TAILQ_NEXT(span->row, entries))
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, 0));

	tbl->part = TBL_PART_LAYOUT;
	return(1);
}
