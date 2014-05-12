/* $OpenBSD: ohash_do.c,v 1.1 2014/05/12 19:09:00 espie Exp $ */
/* ex:ts=8 sw=4: 
 */

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

#include <limits.h>
#include "ohash_int.h"

static void ohash_resize(struct ohash *);

static void 
ohash_resize(struct ohash *h)
{
	struct _ohash_record *n;
	size_t ns;
	unsigned int 	j;
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
#ifdef STATS_HASH
	STAT_HASH_EXPAND++;
	STAT_HASH_SIZE += ns - h->size;
#endif

	n = (h->info.calloc)(ns, sizeof(struct _ohash_record), h->info.data);
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
	(h->info.free)(h->t, h->info.data);
	h->t = n;
	h->size = ns;
	h->total -= h->deleted;
	h->deleted = 0;
}

void *
ohash_remove(struct ohash *h, unsigned int i)
{
	void 		*result = (void *)h->t[i].p;

	if (result == NULL || result == DELETED)
		return NULL;

#ifdef STATS_HASH
	STAT_HASH_ENTRIES--;
#endif
	h->t[i].p = DELETED;
	h->deleted++;
	if (h->deleted >= MINDELETED && 4 * h->deleted > h->total)
		ohash_resize(h);
	return result;
}

void *
ohash_find(struct ohash *h, unsigned int i)
{
	if (h->t[i].p == DELETED)
		return NULL;
	else
		return (void *)h->t[i].p;
}

void *
ohash_insert(struct ohash *h, unsigned int i, void *p)
{
#ifdef STATS_HASH
	STAT_HASH_ENTRIES++;
#endif
	if (h->t[i].p == DELETED) {
		h->deleted--;
		h->t[i].p = p;
	} else {
		h->t[i].p = p;
	/* Arbitrary resize boundary.  Tweak if not efficient enough.  */
		if (++h->total * 4 > h->size * 3)
			ohash_resize(h);
	}
    	return p;
}
