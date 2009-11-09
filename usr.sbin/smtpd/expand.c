/*	$OpenBSD: expand.c,v 1.3 2009/11/09 23:49:34 gilles Exp $	*/

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include "smtpd.h"

struct expand_node *
expandtree_lookup(struct expandtree *expandtree, struct expand_node *node)
{
	struct expand_node key;

	key = *node;
	return RB_FIND(expandtree, expandtree, &key);
}

void
expandtree_increment_node(struct expandtree *expandtree, struct expand_node *node)
{
	struct expand_node *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL) {
		p = calloc(1, sizeof(struct expand_node));
		if (p == NULL)
			fatal("calloc");
		*p = *node;
		if (RB_INSERT(expandtree, expandtree, p))
			fatalx("expandtree_increment_node: node already exists");
	}
	p->refcnt++;
}

void
expandtree_decrement_node(struct expandtree *expandtree, struct expand_node *node)
{
	struct expand_node *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL)
		fatalx("expandtree_decrement_node: node doesn't exist.");

	p->refcnt--;
}

void
expandtree_remove_node(struct expandtree *expandtree, struct expand_node *node)
{
	struct expand_node *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL)
		fatalx("expandtree_remove: node doesn't exist.");

	RB_REMOVE(expandtree, expandtree, p);
}

int
expand_cmp(struct expand_node *e1, struct expand_node *e2)
{
	if (e1->type < e2->type)
		return -1;

	if (e1->type > e2->type)
		return 1;

	return memcmp(&e1->u, &e2->u, sizeof(e1->u));
}

RB_GENERATE(expandtree, expand_node, entry, expand_cmp);
