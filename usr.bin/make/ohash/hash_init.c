/* $OpenBSD: hash_init.c,v 1.2 2000/06/28 10:12:47 espie Exp $ */
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

void 
hash_init(h, size, info)
	struct hash 	*h;
	unsigned int 	size;
	struct hash_info*info;
{
	h->size = 1UL << size;
	if (h->size < MINSIZE)
		h->size = MINSIZE;
#ifdef STATS_HASH
	STAT_HASH_CREATION++;
	STAT_HASH_SIZE += h->size;
#endif
	/* Copy info so that caller may free it.  */
	h->info.key_offset = info->key_offset;
	h->info.halloc = info->halloc;
	h->info.hfree = info->hfree;
	h->info.alloc = info->alloc;
	h->info.data = info->data;
	h->t = (h->info.halloc)(sizeof(struct hash_record) * h->size,
	    h->info.data);
	h->total = h->deleted = 0;
}

