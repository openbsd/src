/*	$OpenBSD: tree.c,v 1.3 2013/01/26 09:37:24 gilles Exp $	*/

/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/tree.h>

#include <sys/socket.h>	/* for smtpd.h */
#include <sys/queue.h>	/* for smtpd.h */
#include <stdio.h>	/* for smtpd.h */
#include <imsg.h>	/* for smtpd.h */

#include <err.h>
#include <inttypes.h>
#include <stdlib.h>

#include "smtpd.h"

struct treeentry {
	SPLAY_ENTRY(treeentry)	 entry;
	uint64_t		 id;
	void			*data;
};

static int treeentry_cmp(struct treeentry *, struct treeentry *);

SPLAY_PROTOTYPE(tree, treeentry, entry, treeentry_cmp);

int
tree_check(struct tree *t, uint64_t id)
{
	struct treeentry	key;

	key.id = id;
	return (SPLAY_FIND(tree, t, &key) != NULL);
}

void *
tree_set(struct tree *t, uint64_t id, void *data)
{
	struct treeentry	*entry, key;
	char			*old;

	key.id = id;
	if ((entry = SPLAY_FIND(tree, t, &key)) == NULL) {
		entry = xmalloc(sizeof *entry, "tree_set");
		entry->id = id;
		SPLAY_INSERT(tree, t, entry);
		old = NULL;
	} else
		old = entry->data;

	entry->data = data;

	return (old);
}

void
tree_xset(struct tree *t, uint64_t id, void *data)
{
	struct treeentry	*entry;

	entry = xmalloc(sizeof *entry, "tree_xset");
	entry->id = id;
	entry->data = data;
	if (SPLAY_INSERT(tree, t, entry))
		errx(1, "tree_xset(%p, 0x%016"PRIx64 ")", t, id);
}

void *
tree_get(struct tree *t, uint64_t id)
{
	struct treeentry	key, *entry;

	key.id = id;
	if ((entry = SPLAY_FIND(tree, t, &key)) == NULL)
		return (NULL);

	return (entry->data);
}

void *
tree_xget(struct tree *t, uint64_t id)
{
	struct treeentry	key, *entry;

	key.id = id;
	if ((entry = SPLAY_FIND(tree, t, &key)) == NULL)
		errx(1, "tree_get(%p, 0x%016"PRIx64 ")", t, id);

	return (entry->data);
}

void *
tree_pop(struct tree *t, uint64_t id)
{
	struct treeentry	key, *entry;
	void			*data;

	key.id = id;
	if ((entry = SPLAY_FIND(tree, t, &key)) == NULL)
		return (NULL);

	data = entry->data;
	SPLAY_REMOVE(tree, t, entry);
	free(entry);

	return (data);
}

void *
tree_xpop(struct tree *t, uint64_t id)
{
	struct treeentry	key, *entry;
	void			*data;

	key.id = id;
	if ((entry = SPLAY_FIND(tree, t, &key)) == NULL)
		errx(1, "tree_xpop(%p, 0x%016" PRIx64 ")", t, id);

	data = entry->data;
	SPLAY_REMOVE(tree, t, entry);
	free(entry);

	return (data);
}

int
tree_poproot(struct tree *t, uint64_t *id, void **data)
{
	struct treeentry	*entry;

	entry = SPLAY_ROOT(t);
	if (entry == NULL)
		return (0);
	if (id)
		*id = entry->id;
	if (data)
		*data = entry->data;
	SPLAY_REMOVE(tree, t, entry);
	free(entry);
	return (1);
}

int
tree_root(struct tree *t, uint64_t *id, void **data)
{
	struct treeentry	*entry;

	entry = SPLAY_ROOT(t);
	if (entry == NULL)
		return (0);
	if (id)
		*id = entry->id;
	if (data)
		*data = entry->data;
	return (1);
}

int
tree_iter(struct tree *t, void **hdl, uint64_t *id, void **data)
{
	struct treeentry *curr = *hdl;

	if (curr == NULL)
		curr = SPLAY_MIN(tree, t);
	else
		curr = SPLAY_NEXT(tree, t, curr);

	if (curr) {
		*hdl = curr;
		if (id)
			*id = curr->id;
		if (data)
			*data = curr->data;
		return (1);
	}

	return (0);
}

int
tree_iterfrom(struct tree *t, void **hdl, uint64_t k, uint64_t *id, void **data)
{
	struct treeentry *curr = *hdl, key;

	if (curr == NULL) {
		if (k == 0)
			curr = SPLAY_MIN(tree, t);
		else {
			key.id = k;
			curr = SPLAY_FIND(tree, t, &key);
			if (curr == NULL) {
				SPLAY_INSERT(tree, t, &key);
				curr = SPLAY_NEXT(tree, t, &key);
				SPLAY_REMOVE(tree, t, &key);
			}
		}
	} else
		curr = SPLAY_NEXT(tree, t, curr);

	if (curr) {
		*hdl = curr;
		if (id)
			*id = curr->id;
		if (data)
			*data = curr->data;
		return (1);
	}

	return (0);
}

void
tree_merge(struct tree *dst, struct tree *src)
{
	struct treeentry	*entry;

	while (!SPLAY_EMPTY(src)) {
		entry = SPLAY_ROOT(src);
		SPLAY_REMOVE(tree, src, entry);
		if (SPLAY_INSERT(tree, dst, entry))
			errx(1, "tree_merge: duplicate");
	}
}

static int
treeentry_cmp(struct treeentry *a, struct treeentry *b)
{
	if (a->id < b->id)
		return (-1);
	if (a->id > b->id)
		return (1);
	return (0);
}

SPLAY_GENERATE(tree, treeentry, entry, treeentry_cmp);
