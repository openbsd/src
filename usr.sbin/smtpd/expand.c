/*	$OpenBSD: expand.c,v 1.13 2012/09/19 09:06:35 eric Exp $	*/

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

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

struct expandnode *
expand_lookup(struct expandtree *expandtree, struct expandnode *key)
{
	return RB_FIND(expandtree, expandtree, key);
}

void
expand_insert(struct expandtree *expandtree, struct expandnode *node)
{
	struct expandnode *p;

	if (expand_lookup(expandtree, node))
		return;

	p = xmemdup(node, sizeof *p, "expand_insert");
	RB_INSERT(expandtree, expandtree, p);
}

void
expand_free(struct expandtree *expandtree)
{
	struct expandnode *xn;

	while ((xn = RB_ROOT(expandtree)) != NULL) {
		RB_REMOVE(expandtree, expandtree, xn);
		free(xn);
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
