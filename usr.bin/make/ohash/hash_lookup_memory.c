/* $OpenBSD: hash_lookup_memory.c,v 1.1 2000/06/23 16:24:50 espie Exp $ */
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

unsigned 
hash_lookup_memory(h, k, size, hv)
	struct hash 	*h;
	const char 	*k;
	size_t		size;
	u_int32_t	hv;
{
	unsigned i, incr;
	unsigned empty;
	
#ifdef STATS_HASH
	STAT_HASH_LOOKUP++;
#endif
	empty = NONE;
	i = hv % h->size;
	incr = ((hv % (h->size-2)) & ~1) + 1;
	while (h->t[i].p != NULL) {
#ifdef STATS_HASH
		STAT_HASH_LENGTH++;
#endif
		if (h->t[i].p == DELETED) {
			if (empty == NONE)
				empty = i;
		} else if (h->t[i].hv == hv && 
		    memcmp(h->t[i].p+h->info.key_offset, k, size) == 0) {
		    	if (empty != NONE) {
				h->t[empty].hv = hv;
				h->t[empty].p = h->t[i].p;
				h->t[i].p = DELETED;
				return empty;
			} else {
#ifdef STATS_HASH
				STAT_HASH_POSITIVE++;
#endif
			}	return i;
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

