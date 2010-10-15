/*	$Id: tbl_tree.c,v 1.2 2010/10/15 21:33:47 schwarze Exp $ */
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

#include "out.h"
#include "term.h"
#include "tbl_extern.h"

static	const char * const htypes[TBL_HEAD_MAX] = {
	"data",
	"vert",
	"dvert",
};

static	const char * const ctypes[TBL_CELL_MAX] = {
	"centre",
	"right",
	"left",
	"number",
	"span",
	"long",
	"down",
	"horiz",
	"dhoriz",
	"vert",
	"dvert",
};


/* ARGSUSED */
int
tbl_calc_tree(struct tbl *tbl)
{

	return(1);
}


int
tbl_write_tree(const struct tbl *tbl)
{
	struct tbl_row	*row;
	struct tbl_cell	*cell;
	struct tbl_span	*span;
	struct tbl_data	*data;
	struct tbl_head	*head;

	(void)printf("header\n");
	TAILQ_FOREACH(head, &tbl->head, entries)
		(void)printf("\t%s (=%p)\n", htypes[head->pos], head);

	(void)printf("layout\n");
	TAILQ_FOREACH(row, &tbl->row, entries) {
		(void)printf("\trow (=%p)\n", row);
		TAILQ_FOREACH(cell, &row->cell, entries)
			(void)printf("\t\t%s (=%p) >%p\n", 
					ctypes[cell->pos], 
					cell, cell->head);
	}

	(void)printf("data\n");
	TAILQ_FOREACH(span, &tbl->span, entries) {
		(void)printf("\tspan >%p\n", span->row);
		TAILQ_FOREACH(data, &span->data, entries)
			(void)printf("\t\tdata >%p\n", data->cell);
	}

	return(1);
}
