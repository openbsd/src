/*	$OpenBSD: map.c,v 1.1 2020/01/21 16:24:55 mpi Exp $ */

/*
 * Copyright (c) 2020 Martin Pieuchot <mpi@openbsd.org>
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

/*
 * Associative array implemented with RB-Tree.
 */

#include <sys/queue.h>
#include <sys/tree.h>

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bt_parser.h"
#include "btrace.h"

RB_HEAD(mtree, mentry);

#define	KLEN 64

struct mentry {
	RB_ENTRY(mentry)	 mlink;
	char			 mkey[KLEN];
	struct bt_arg		*mval;
};

int	mcmp(struct mentry *, struct mentry *);

RB_GENERATE(mtree, mentry, mlink, mcmp);

int
mcmp(struct mentry *me0, struct mentry *me1)
{
	return strncmp(me0->mkey, me1->mkey, KLEN - 1);
}

void
map_clear(struct bt_var *bv)
{
	struct mtree *map = (struct mtree *)bv->bv_value;
	struct mentry *mep;

	while ((mep = RB_MIN(mtree, map)) != NULL) {
		RB_REMOVE(mtree, map, mep);
		free(mep);
	}

	assert(RB_EMPTY(map));
	free(map);

	bv->bv_value = NULL;
}

void
map_delete(struct bt_var *bv, const char *key)
{
	struct mtree *map = (struct mtree *)bv->bv_value;
	struct mentry me, *mep;

	strlcpy(me.mkey, key, KLEN);
	mep = RB_REMOVE(mtree, map, &me);
	free(mep);
}

void
map_insert(struct bt_var *bv, const char *key, struct bt_arg *bval)
{
	struct mtree *map = (struct mtree *)bv->bv_value;
	struct mentry me, *mep;
	long val;

	if (map == NULL) {
		map = calloc(1, sizeof(struct mtree));
		if (map == NULL)
			err(1, "mtree: calloc");
		bv->bv_value = (struct bt_arg *)map;
	}

	strlcpy(me.mkey, key, KLEN);
	mep = RB_FIND(mtree, map, &me);
	if (mep == NULL) {
		mep = calloc(1, sizeof(struct mentry));
		if (mep == NULL)
			err(1, "mentry: calloc");

		strlcpy(mep->mkey, key, KLEN);
		RB_INSERT(mtree, map, mep);
	}

	switch (bval->ba_type) {
	case B_AT_MF_COUNT:
		if (mep->mval == NULL)
			mep->mval = ba_new(0, B_AT_LONG);
		val = (long)mep->mval->ba_value;
		val++;
		mep->mval->ba_value = (void *)val;
		break;
	case B_AT_STR:
	case B_AT_LONG:
		mep->mval = bval;
		break;
	default:
		errx(1, "no insert support for type %d", bval->ba_type);
	}
}

/* Print at most `top' entries of the map ordered by value. */
void
map_print(struct bt_var *bv, size_t top)
{
	static struct bt_arg nullba = { { NULL }, (void *)0, B_AT_LONG };
	static struct bt_arg maxba = { { NULL }, (void *)LONG_MAX, B_AT_LONG };
	struct mtree *map = (void *)bv->bv_value;
	struct mentry *mep, *mcur;
	struct bt_arg *bhigh, *bprev;
	size_t i;

	if (map == NULL)
		return;

	bprev = &maxba;
	for (i = 0; i < top; i++) {
		mcur = NULL;
		bhigh = &nullba;
		RB_FOREACH(mep, mtree, map) {
			if (bacmp(mep->mval, bhigh) >= 0 &&
			    bacmp(mep->mval, bprev) < 0 &&
			    mep->mval != bprev) {
				mcur = mep;
				bhigh = mcur->mval;
			}
		}
		if (mcur == NULL)
			break;
		printf("@map[%s]: %s\n", mcur->mkey, ba2str(mcur->mval, NULL));
		bprev = mcur->mval;
	}
}

void
map_zero(struct bt_var *bv)
{
	struct mtree *map = (struct mtree *)bv->bv_value;
	struct mentry *mep;

	RB_FOREACH(mep, mtree, map) {
		mep->mval->ba_value = 0;
		mep->mval->ba_type = B_AT_LONG;
	}
}
