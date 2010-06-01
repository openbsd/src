/*	$OpenBSD: expand.c,v 1.7 2010/06/01 19:47:08 jacekm Exp $	*/

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

#include "smtpd.h"

struct expandnode *
expandtree_lookup(struct expandtree *expandtree, struct expandnode *node)
{
	struct expandnode key;

	key = *node;
	return RB_FIND(expandtree, expandtree, &key);
}

void
expandtree_increment_node(struct expandtree *expandtree, struct expandnode *node)
{
	struct expandnode *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL) {
		p = calloc(1, sizeof(struct expandnode));
		if (p == NULL)
			fatal("calloc");
		*p = *node;
		if (RB_INSERT(expandtree, expandtree, p))
			fatalx("expandtree_increment_node: node already exists");
	}
	p->refcnt++;
}

void
expandtree_decrement_node(struct expandtree *expandtree, struct expandnode *node)
{
	struct expandnode *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL)
		fatalx("expandtree_decrement_node: node doesn't exist.");

	p->refcnt--;
}

void
expandtree_remove_node(struct expandtree *expandtree, struct expandnode *node)
{
	struct expandnode *p;

	p = expandtree_lookup(expandtree, node);
	if (p == NULL)
		fatalx("expandtree_remove: node doesn't exist.");

	RB_REMOVE(expandtree, expandtree, p);
}

void
expandtree_free_nodes(struct expandtree *expandtree)
{
	struct expandnode *p;
	struct expandnode *nxt;

	for (p = RB_MIN(expandtree, expandtree); p != NULL; p = nxt) {
		nxt = RB_NEXT(expandtree, expandtree, p);
		RB_REMOVE(expandtree, expandtree, p);
		free(p);
	}
}

int
expand_cmp(struct expandnode *e1, struct expandnode *e2)
{
	if (e1->type < e2->type)
		return -1;

	if (e1->type > e2->type)
		return 1;

	return memcmp(&e1->u, &e2->u, sizeof(e1->u));
}

RB_GENERATE(expandtree, expandnode, entry, expand_cmp);
