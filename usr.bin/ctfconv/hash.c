/*	$OpenBSD: hash.c,v 1.2 2017/08/11 14:58:56 jasper Exp $ */

/* Copyright (c) 1999, 2004 Marc Espie <espie@openbsd.org>
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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "hash.h"

struct _hash_record {
	uint32_t	hv;
	struct hash_entry	*p;
};

struct hash {
	struct _hash_record 	*t;
	unsigned int 		size;
	unsigned int 		total;
	unsigned int 		deleted;
};

#define DELETED		((struct hash_entry *)h)
#define NONE		(h->size)

/* Don't bother changing the hash table if the change is small enough.  */
#define MINSIZE		(1UL << 4)
#define MINDELETED	4

static void hash_resize(struct hash *);
static uint32_t hash_interval(const char *, const char **);
static unsigned int hash_qlookup(struct hash *, const char *);


/* hash_delete only frees the hash structure. Use hash_first/hash_next
 * to free entries as well.  */
void
hash_delete(struct hash *h)
{
	free(h->t);
	h->t = NULL;
}

static void
hash_resize(struct hash *h)
{
	struct _hash_record *n;
	size_t ns;
	unsigned int	j;
	unsigned int	i, incr;

	if (4 * h->deleted < h->total) {
		if (h->size >= (UINT_MAX >> 1U))
			ns = UINT_MAX;
		else
			ns = h->size << 1U;
	} else if (3 * h->deleted > 2 * h->total)
		ns = h->size >> 1U;
	else
		ns = h->size;
	if (ns < MINSIZE)
		ns = MINSIZE;

	n = calloc(ns, sizeof(struct _hash_record));
	if (!n)
		return;

	for (j = 0; j < h->size; j++) {
		if (h->t[j].p != NULL && h->t[j].p != DELETED) {
			i = h->t[j].hv % ns;
			incr = ((h->t[j].hv % (ns - 2)) & ~1) + 1;
			while (n[i].p != NULL) {
				i += incr;
				if (i >= ns)
					i -= ns;
			}
			n[i].hv = h->t[j].hv;
			n[i].p = h->t[j].p;
		}
	}
	free(h->t);
	h->t = n;
	h->size = ns;
	h->total -= h->deleted;
	h->deleted = 0;
}

void *
hash_remove(struct hash *h, unsigned int i)
{
	void		*result = (void *)h->t[i].p;

	if (result == NULL || result == DELETED)
		return NULL;

	h->t[i].p = DELETED;
	h->deleted++;
	if (h->deleted >= MINDELETED && 4 * h->deleted > h->total)
		hash_resize(h);
	return result;
}

void
hash_insert(struct hash *h, unsigned int i, struct hash_entry *p,
    const char *key)
{
	p->hkey = key;

	if (h->t[i].p == DELETED) {
		h->deleted--;
		h->t[i].p = p;
	} else {
		h->t[i].p = p;
		/* Arbitrary resize boundary.  Tweak if not efficient enough. */
		if (++h->total * 4 > h->size * 3)
			hash_resize(h);
	}
}

void *
hash_first(struct hash *h, unsigned int *pos)
{
	*pos = 0;
	return hash_next(h, pos);
}

void *
hash_next(struct hash *h, unsigned int *pos)
{
	for (; *pos < h->size; (*pos)++)
		if (h->t[*pos].p != DELETED && h->t[*pos].p != NULL)
			return (void *)h->t[(*pos)++].p;
	return NULL;
}

struct hash *
hash_init(unsigned int size)
{
	struct hash *h;

	h = calloc(1, sizeof(*h));
	if (h == NULL)
		return NULL;

	h->size = 1UL << size;
	if (h->size < MINSIZE)
		h->size = MINSIZE;
	/* Copy info so that caller may free it.  */
	h->total = h->deleted = 0;
	h->t = calloc(h->size, sizeof(struct _hash_record));
	if (h->t == NULL) {
		free(h);
		return NULL;
	}

	return h;
}

static uint32_t
hash_interval(const char *s, const char **e)
{
	uint32_t k;

	if (!*e)
		*e = s + strlen(s);
	if (s == *e)
		k = 0;
	else
		k = *s++;
	while (s != *e)
		k =  ((k << 2) | (k >> 30)) ^ *s++;
	return k;
}

static unsigned int
hash_qlookup(struct hash *h, const char *start)
{
	const char *end = NULL;
	unsigned int i, incr;
	unsigned int empty;
	uint32_t hv;

	hv = hash_interval(start, &end);

	empty = NONE;
	i = hv % h->size;
	incr = ((hv % (h->size-2)) & ~1) + 1;
	while (h->t[i].p != NULL) {
		if (h->t[i].p == DELETED) {
			if (empty == NONE)
				empty = i;
		} else if (h->t[i].hv == hv &&
		    strncmp(h->t[i].p->hkey, start, end - start) == 0 &&
		    (h->t[i].p->hkey)[end-start] == '\0') {
			if (empty != NONE) {
				h->t[empty].hv = hv;
				h->t[empty].p = h->t[i].p;
				h->t[i].p = DELETED;
				return empty;
			} else {
				return i;
			}
		}
		i += incr;
		if (i >= h->size)
			i -= h->size;
	}

	/* Found an empty position.  */
	if (empty != NONE)
		i = empty;
	h->t[i].hv = hv;
	return i;
}

struct hash_entry *
hash_find(struct hash *h, const char *start, unsigned int *slot)
{
	unsigned int i;

	i = hash_qlookup(h, start);
	if (slot != NULL)
		*slot = i;

	if (h->t[i].p == DELETED)
		return NULL;

	return h->t[i].p;
}
