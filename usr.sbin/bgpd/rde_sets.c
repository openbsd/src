/*	$OpenBSD: rde_sets.c,v 1.2 2018/09/08 09:29:25 claudio Exp $ */

/*
 * Copyright (c) 2018 Claudio Jeker <claudio@openbsd.org>
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rde.h"

struct as_set {
	char			 name[SET_NAME_LEN];
	u_int32_t		*set;
	SIMPLEQ_ENTRY(as_set)	 entry;
	size_t			 nmemb;
	size_t			 max;
	int			 dirty;
};

void
as_sets_insert(struct as_set_head *as_sets, struct as_set *aset)
{
	SIMPLEQ_INSERT_TAIL(as_sets, aset, entry);
}

struct as_set *
as_sets_lookup(struct as_set_head *as_sets, const char *name)
{
	struct as_set *aset;

	SIMPLEQ_FOREACH(aset, as_sets, entry) {
		if (strcmp(aset->name, name) == 0)
			return aset;
	}
	return NULL;
}

static void
as_set_free(struct as_set *aset)
{
	free(aset->set);
	free(aset);
}

void
as_sets_free(struct as_set_head *as_sets)
{
	struct as_set *aset;

	if (as_sets == NULL)
		return;
	while (!SIMPLEQ_EMPTY(as_sets)) {
		aset = SIMPLEQ_FIRST(as_sets);
		SIMPLEQ_REMOVE_HEAD(as_sets, entry);
		as_set_free(aset);
	}
	free(as_sets);
}

void
print_as_sets(struct as_set_head *as_sets)
{
	struct as_set *aset;
	size_t i;
	int len;

	if (as_sets == NULL)
		return;
	SIMPLEQ_FOREACH(aset, as_sets, entry) {
		printf("as-set \"%s\" {\n\t", aset->name);
		for (i = 0, len = 8; i < aset->nmemb; i++) {
			if (len > 72) {
				printf("\n\t");
				len = 8;
			}
			len += printf("%u ", aset->set[i]);
		}
		printf("\n}\n\n");
	}
}

int
as_sets_send(struct imsgbuf *ibuf, struct as_set_head *as_sets)
{
	struct as_set *aset;
	struct ibuf *wbuf;
	size_t i, l;

	if (as_sets == NULL)
		return 0;
	SIMPLEQ_FOREACH(aset, as_sets, entry) {
		if ((wbuf = imsg_create(ibuf, IMSG_RECONF_AS_SET, 0, 0,
		    sizeof(aset->nmemb) + sizeof(aset->name))) == NULL)
			return -1;
		if (imsg_add(wbuf, &aset->nmemb, sizeof(aset->nmemb)) == -1 ||
		    imsg_add(wbuf, aset->name, sizeof(aset->name)) == -1)
			return -1;
		imsg_close(ibuf, wbuf);

		for (i = 0; i < aset->nmemb; i += l) {
			l = (aset->nmemb - i > 1024 ? 1024 : aset->nmemb - i);

			if (imsg_compose(ibuf, IMSG_RECONF_AS_SET_ITEMS, 0, 0,
			    -1, aset->set + i, l * sizeof(*aset->set)) == -1)
				return -1;
		}

		if (imsg_compose(ibuf, IMSG_RECONF_AS_SET_DONE, 0, 0, -1,
		    NULL, 0) == -1)
			return -1;
	}
	return 0;
}

void
as_sets_mark_dirty(struct as_set_head *old, struct as_set_head *new)
{
	struct as_set	*n, *o;

	SIMPLEQ_FOREACH(n, new, entry) {
		if (old == NULL || (o = as_sets_lookup(old, n->name)) == NULL ||
		    !as_set_equal(n, o))
			n->dirty = 1;
	}
}

struct as_set *
as_set_new(const char *name, size_t nmemb)
{
	struct as_set *aset;
	size_t len;

	aset = calloc(1, sizeof(*aset));
	if (aset == NULL)
		return NULL;

	len = strlcpy(aset->name, name, sizeof(aset->name));
	assert(len < sizeof(aset->name));

	if (nmemb == 0)
		nmemb = 16;

	aset->max = nmemb;
	aset->set = calloc(nmemb, sizeof(*aset->set));
	if (aset->set == NULL) {
		free(aset);
		return NULL;
	}

	return aset;
}

int
as_set_add(struct as_set *aset, u_int32_t *elms, size_t nelms)
{
	if (aset->max < nelms || aset->max - nelms < aset->nmemb) {
		u_int32_t *s;
		size_t new_size;

		if (aset->nmemb >= SIZE_T_MAX - 4096 - nelms) {
			errno = ENOMEM;
			return -1;
		}
		for (new_size = aset->max; new_size < aset->nmemb + nelms; )
		     new_size += (new_size < 4096 ? new_size : 4096);

		s = reallocarray(aset->set, new_size, sizeof(*aset->set));
		if (s == NULL)
			return -1;
		aset->set = s;
		aset->max = new_size;
	}

	memcpy(aset->set + aset->nmemb, elms, nelms * sizeof(*elms));
	aset->nmemb += nelms;

	return 0;
}

static int
as_set_cmp(const void *ap, const void *bp)
{
	const u_int32_t *a = ap;
	const u_int32_t *b = bp;

	if (*a > *b)
		return 1;
	else if (*a < *b)
		return -1;
	return 0;
}

void
as_set_prep(struct as_set *aset)
{
	qsort(aset->set, aset->nmemb, sizeof(*aset->set), as_set_cmp);
}

int
as_set_match(const struct as_set *a, u_int32_t asnum)
{
	if (bsearch(&asnum, a->set, a->nmemb, sizeof(asnum), as_set_cmp))
		return 1;
	else
		return 0;
}

int
as_set_equal(const struct as_set *a, const struct as_set *b)
{
	if (a->nmemb != b->nmemb)
		return 0;
	if (memcmp(a->set, b->set, a->nmemb * sizeof(*a->set)) != 0)
		return 0;
	return 1;
}

int
as_set_dirty(const struct as_set *a)
{
	return (a->dirty);
}
