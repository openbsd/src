/* $OpenBSD: hash_do.c,v 1.2 2000/06/28 10:12:46 espie Exp $ */
/* ex:ts=8 sw=4: 
 */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Code written for the OpenBSD project.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ohash_int.h"

static void hash_resize __P((struct hash *));

static void 
hash_resize(h)
	struct hash 	*h;
{
	struct hash_record *n;
	unsigned int 	ns, j;
	unsigned int	i, incr;

	if (4 * h->deleted < h->total)
		ns = h->size << 1;
	else if (3 * h->deleted > 2 * h->total)
		ns = h->size >> 1;
	else
		ns = h->size;
	if (ns < MINSIZE)
		ns = MINSIZE;
#ifdef STATS_HASH
	STAT_HASH_EXPAND++;
	STAT_HASH_SIZE += ns - h->size;
#endif
	n = (h->info.halloc)(sizeof(struct hash_record) * ns, h->info.data);
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
	(h->info.hfree)(h->t, sizeof(struct hash_record) * h->size, 
		h->info.data);
	h->t = n;
	h->size = ns;
	h->total -= h->deleted;
	h->deleted = 0;
}

void *
hash_remove(h, i)
	struct hash 	*h;
	unsigned int 	i;
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
		hash_resize(h);
	return result;
}

void *
hash_find(h, i)
	struct hash 	*h;
	unsigned int 	i;
{
	if (h->t[i].p == DELETED)
		return NULL;
	else
		return (void *)h->t[i].p;
}

void *
hash_insert(h, i, p)
	struct hash 	*h;
	unsigned int 	i;
	void 		*p;
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
			hash_resize(h);
	}
    	return p;
}

