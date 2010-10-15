/*	$Id: tbl.c,v 1.3 2010/10/15 22:07:12 schwarze Exp $ */
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "out.h"
#include "term.h"
#include "tbl.h"
#include "tbl_extern.h"


const	char *const	 errnames[ERR_MAX] = {
	"bad syntax",	 /* ERR_SYNTAX */
	"bad option"	 /* ERR_OPTION */
};

static	char		 buf[1024]; /* XXX */

static	enum tbl_tok	 tbl_next_char(char);
static	void		 tbl_init(struct tbl *);
static	void		 tbl_clear(struct tbl *);
static	struct tbl_head *tbl_head_alloc(struct tbl *);
static	void		 tbl_span_free(struct tbl_span *);
static	void		 tbl_data_free(struct tbl_data *);
static	void		 tbl_row_free(struct tbl_row *);

static	void		 headadj(const struct tbl_cell *, 
				struct tbl_head *);

static void
tbl_init(struct tbl *tbl)
{

	bzero(tbl, sizeof(struct tbl));

	tbl->part = TBL_PART_OPTS;
	tbl->tab = '\t';
	tbl->linesize = 12;
	tbl->decimal = '.';

	TAILQ_INIT(&tbl->span);
	TAILQ_INIT(&tbl->row);
	TAILQ_INIT(&tbl->head);
}


int
tbl_read(struct tbl *tbl, const char *f, int ln, const char *p, int len)
{
	
	if (len && TBL_PART_OPTS == tbl->part)
		if (';' != p[len - 1])
			tbl->part = TBL_PART_LAYOUT;

	switch (tbl->part) {
	case (TBL_PART_OPTS):
		return(tbl_option(tbl, f, ln, p));
	case (TBL_PART_CLAYOUT):
		/* FALLTHROUGH */
	case (TBL_PART_LAYOUT):
		return(tbl_layout(tbl, f, ln, p));
	case (TBL_PART_DATA):
		return(tbl_data(tbl, f, ln, p));
	case (TBL_PART_ERROR):
		break;
	}

	return(0);
}


int
tbl_close(struct termp *p, struct tbl *tbl, const char *f, int ln)
{

	if (TBL_PART_DATA != tbl->part) 
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, 0));
	if ( ! tbl_data_close(tbl, f, ln))
		return(0);
#if 1
	return(tbl_calc_term(p, tbl));
#else
	return(tbl_calc_tree(tbl));
#endif
}


int
tbl_write(struct termp *p, const struct tbl *tbl)
{

#if 1
	return(tbl_write_term(p, tbl));
#else
	return(tbl_write_tree(tbl));
#endif
}


static enum tbl_tok
tbl_next_char(char c)
{

	/*
	 * These are delimiting tokens.  They separate out words in the
	 * token stream.
	 */

	switch (c) {
	case ('('):
		return(TBL_TOK_OPENPAREN);
	case (')'):
		return(TBL_TOK_CLOSEPAREN);
	case (' '):
		return(TBL_TOK_SPACE);
	case ('\t'):
		return(TBL_TOK_TAB);
	case (';'):
		return(TBL_TOK_SEMICOLON);
	case ('.'):
		return(TBL_TOK_PERIOD);
	case (','):
		return(TBL_TOK_COMMA);
	case (0):
		return(TBL_TOK_NIL);
	default:
		break;
	}

	return(TBL_TOK_WORD);
}


const char *
tbl_last(void)
{

	return(buf);
}


int
tbl_last_uint(void)
{
	char		*ep;
	long		 lval;

	/* From OpenBSD's strtol(3).  Gross. */

	errno = 0;
	lval = strtol(buf, &ep, 10);
	if (buf[0] == 0 || *ep != 0)
		return(-1);
	if (errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
		return(-1);
	if (lval < 0 || lval > INT_MAX)
		return(-1);

	return((int)lval);
}


enum tbl_tok
tbl_next(const char *p, int *pos)
{
	int		 i;
	enum tbl_tok	 c;

	buf[0] = 0;

	if (TBL_TOK_WORD != (c = tbl_next_char(p[*pos]))) {
		if (TBL_TOK_NIL != c) {
			buf[0] = p[*pos];
			buf[1] = 0;
			(*pos)++;
		}
		return(c);
	}

	/*
	 * Copy words into a nil-terminated buffer.  For now, we use a
	 * static buffer.  Eventually this should be made into a dynamic
	 * one living in struct tbl.
	 */

	for (i = 0; i < 1023; i++, (*pos)++)
		if (TBL_TOK_WORD == tbl_next_char(p[*pos]))
			buf[i] = p[*pos];
		else
			break;

	assert(i < 1023);
	buf[i] = 0;

	return(TBL_TOK_WORD);
}


int
tbl_err(struct tbl *tbl)
{

	(void)fprintf(stderr, "%s\n", strerror(errno));
	tbl->part = TBL_PART_ERROR;
	return(0);
}


/* ARGSUSED */
int
tbl_warnx(struct tbl *tbl, enum tbl_err tok, 
		const char *f, int line, int pos)
{

	(void)fprintf(stderr, "%s:%d:%d: %s\n", 
			f, line, pos + 1, errnames[tok]);

	/* TODO: -Werror */
	return(1);
}


int
tbl_errx(struct tbl *tbl, enum tbl_err tok, 
		const char *f, int line, int pos)
{

	(void)fprintf(stderr, "%s:%d:%d: %s\n", 
			f, line, pos + 1, errnames[tok]);

	tbl->part = TBL_PART_ERROR;
	return(0);
}


struct tbl *
tbl_alloc(void)
{
	struct tbl	*p;

	if (NULL == (p = malloc(sizeof(struct tbl))))
		return(NULL);

	tbl_init(p);
	return(p);
}


void
tbl_free(struct tbl *p)
{

	tbl_clear(p);
	free(p);
}


void
tbl_reset(struct tbl *tbl)
{

	tbl_clear(tbl);
	tbl_init(tbl);
}


struct tbl_span *
tbl_span_alloc(struct tbl *tbl)
{
	struct tbl_span	*p, *pp;
	struct tbl_row	*row;

	if (NULL == (p = calloc(1, sizeof(struct tbl_span)))) {
		(void)tbl_err(tbl);
		return(NULL);
	}

	TAILQ_INIT(&p->data);
	TAILQ_INSERT_TAIL(&tbl->span, p, entries);

	/* LINTED */
	pp = TAILQ_PREV(p, tbl_spanh, entries);

	if (pp) {
		row = TAILQ_NEXT(pp->row, entries);
		if (NULL == row)
			row = pp->row;
	} else {
		row = TAILQ_FIRST(&tbl->row);
	}

	assert(row);
	p->row = row;
	p->tbl = tbl;
	return(p);
}


struct tbl_row *
tbl_row_alloc(struct tbl *tbl)
{
	struct tbl_row	*p;

	if (NULL == (p = calloc(1, sizeof(struct tbl_row)))) {
		(void)tbl_err(tbl);
		return(NULL);
	}

	TAILQ_INIT(&p->cell);
	TAILQ_INSERT_TAIL(&tbl->row, p, entries);
	p->tbl = tbl;
	return(p);
}


static void
headadj(const struct tbl_cell *cell, struct tbl_head *head)
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


static struct tbl_head *
tbl_head_alloc(struct tbl *tbl)
{
	struct tbl_head	*p;

	if (NULL == (p = calloc(1, sizeof(struct tbl_head)))) {
		(void)tbl_err(tbl);
		return(NULL);
	}
	p->tbl = tbl;
	return(p);
}


struct tbl_cell *
tbl_cell_alloc(struct tbl_row *rp, enum tbl_cellt pos)
{
	struct tbl_cell	*p, *pp;
	struct tbl_head	*h, *hp;

	if (NULL == (p = calloc(1, sizeof(struct tbl_cell)))) {
		(void)tbl_err(rp->tbl);
		return(NULL);
	}

	TAILQ_INSERT_TAIL(&rp->cell, p, entries);
	p->pos = pos;
	p->row = rp;

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

	/* LINTED */
	pp = TAILQ_PREV(p, tbl_cellh, entries);

	h = pp ? TAILQ_NEXT(pp->head, entries) : 
		TAILQ_FIRST(&rp->tbl->head);

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
			headadj(p, h);
			p->head = h;
			return(p);
		}

		/* Right-shift headers with a new spanner. */
		if (TBL_HEAD_DATA == h->pos && 
				(TBL_CELL_VERT == p->pos ||
				 TBL_CELL_DVERT == p->pos)) {
			if (NULL == (hp = tbl_head_alloc(rp->tbl)))
				return(NULL);
			TAILQ_INSERT_BEFORE(h, hp, entries);
			headadj(p, hp);
			p->head = hp;
			return(p);
		}

		h = TAILQ_NEXT(h, entries);
		if (h) {
			headadj(p, h);
			p->head = h;
			return(p);
		}

		/* Fall through to default case... */
	}

	if (NULL == (hp = tbl_head_alloc(rp->tbl)))
		return(NULL);
	TAILQ_INSERT_TAIL(&rp->tbl->head, hp, entries);
	headadj(p, hp);
	p->head = hp;
	return(p);
}


struct tbl_data *
tbl_data_alloc(struct tbl_span *sp)
{
	struct tbl_data	*p;
	struct tbl_cell	*cp;
	struct tbl_data	*dp;

	if (NULL == (p = calloc(1, sizeof(struct tbl_data)))) {
		(void)tbl_err(sp->row->tbl);
		return(NULL);
	}

	cp = NULL;
	/* LINTED */
	if (NULL == (dp = TAILQ_LAST(&sp->data, tbl_datah)))
		cp = TAILQ_FIRST(&sp->row->cell);
	else if (dp->cell)
		cp = TAILQ_NEXT(dp->cell, entries);

	TAILQ_INSERT_TAIL(&sp->data, p, entries);

	if (cp && (TBL_CELL_VERT == cp->pos || 
				TBL_CELL_DVERT == cp->pos))
		cp = TAILQ_NEXT(cp, entries);

	p->span = sp;
	p->cell = cp;
	return(p);
}


static void
tbl_clear(struct tbl *p)
{
	struct tbl_span	*span;
	struct tbl_head	*head;
	struct tbl_row	*row;

	/* LINTED */
	while ((span = TAILQ_FIRST(&p->span))) {
		TAILQ_REMOVE(&p->span, span, entries);
		tbl_span_free(span);
	}
	/* LINTED */
	while ((row = TAILQ_FIRST(&p->row))) {
		TAILQ_REMOVE(&p->row, row, entries);
		tbl_row_free(row);
	}
	/* LINTED */
	while ((head = TAILQ_FIRST(&p->head))) {
		TAILQ_REMOVE(&p->head, head, entries);
		free(head);
	}
}


static void
tbl_span_free(struct tbl_span *p)
{
	struct tbl_data	*data;

	/* LINTED */
	while ((data = TAILQ_FIRST(&p->data))) {
		TAILQ_REMOVE(&p->data, data, entries);
		tbl_data_free(data);
	}
	free(p);
}


static void
tbl_data_free(struct tbl_data *p)
{

	if (p->string)
		free(p->string);
	free(p);
}


static void
tbl_row_free(struct tbl_row *p)
{
	struct tbl_cell	*cell;

	/* LINTED */
	while ((cell = TAILQ_FIRST(&p->cell))) {
		TAILQ_REMOVE(&p->cell, cell, entries);
		free(cell);
	}
	free(p);
}
