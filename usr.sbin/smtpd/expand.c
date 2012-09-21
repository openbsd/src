/*	$OpenBSD: expand.c,v 1.16 2012/09/21 19:37:08 eric Exp $	*/

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
expand_lookup(struct expand *expand, struct expandnode *key)
{
	return RB_FIND(expandtree, &expand->tree, key);
}

void
expand_insert(struct expand *expand, struct expandnode *node)
{
	struct expandnode *xn;

	if (expand_lookup(expand, node))
		return;

	xn = xmemdup(node, sizeof *xn, "expand_insert");

	/* copy expansion context on node */
	strlcpy(xn->as_user, expand->user, sizeof xn->as_user);

	RB_INSERT(expandtree, &expand->tree, xn);
}

void
expand_free(struct expand *expand)
{
	struct expandnode *xn;

	while ((xn = RB_ROOT(&expand->tree)) != NULL) {
		RB_REMOVE(expandtree, &expand->tree, xn);
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
