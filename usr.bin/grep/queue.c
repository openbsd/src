/*	$OpenBSD: queue.c,v 1.6 2011/07/08 01:20:24 tedu Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * A really poor man's queue.  It does only what it has to and gets out of
 * Dodge.
 */

#include <sys/param.h>

#include <stdlib.h>
#include <string.h>

#include "grep.h"

typedef struct queue {
	struct queue   *next;
	str_t		data;
} queue_t;

static queue_t	*q_head, *q_tail;
static int	 count;

static queue_t	*dequeue(void);

void
initqueue(void)
{
	q_head = q_tail = NULL;
}

static void
free_item(queue_t *item)
{
	free(item);
}

void
enqueue(str_t *x)
{
	queue_t	*item;

	item = grep_malloc(sizeof *item + x->len);
	item->data.len = x->len;
	item->data.line_no = x->line_no;
	item->data.off = x->off;
	item->data.dat = (char *)item + sizeof *item;
	memcpy(item->data.dat, x->dat, x->len);
	item->data.file = x->file;
	item->next = NULL;

	if (!q_head) {
		q_head = q_tail = item;
	} else {
		q_tail->next = item;
		q_tail = item;
	}

	if (++count > Bflag)
		free_item(dequeue());
}

static queue_t *
dequeue(void)
{
	queue_t	*item;

	if (q_head == NULL)
		return NULL;

	--count;
	item = q_head;
	q_head = item->next;
	if (q_head == NULL)
		q_tail = NULL;
	return item;
}

void
printqueue(void)
{
	queue_t *item;

	while ((item = dequeue()) != NULL) {
		printline(&item->data, '-', NULL);
		free_item(item);
	}
}

void
clearqueue(void)
{
	queue_t	*item;

	while ((item = dequeue()) != NULL)
		free_item(item);
}
