/*	$OpenBSD: hash.c,v 1.1 2008/06/21 15:39:15 joris Exp $	*/
/*
 * Copyright (c) 2008 Joris Vink <joris@openbsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvs.h"
#include "hash.h"
#include "xmalloc.h"

void
hash_table_init(struct hash_table *htable, size_t hsize)
{
	size_t i;
	u_int power;

	if (hsize < MIN_HASH_SIZE)
		hsize = MIN_HASH_SIZE;

	if (hsize > MAX_HASH_SIZE)
		hsize = MAX_HASH_SIZE;

	if ((hsize & (hsize - 1)) != 0) {
		for (power = 0; hsize != 0; power++)
			hsize >>= 1;
		hsize = 1 << power;
	}

	htable->h_table = xcalloc(hsize, sizeof(struct hash_head *));
	htable->h_size = hsize;

	for (i = 0; i < htable->h_size; i++)
		SLIST_INIT(&(htable->h_table[i]));
}

void
hash_table_enter(struct hash_table *htable, struct hash_data *e)
{
	uint32_t hashv;
	struct hash_head *tableh;
	struct hash_table_entry *entry;

	hashv = hash4(e->h_key, strlen(e->h_key));
	tableh = &(htable->h_table[(hashv & (htable->h_size - 1))]);

	entry = xmalloc(sizeof(*entry));
	entry->h_data.h_key = e->h_key;
	entry->h_data.h_data = e->h_data;
	SLIST_INSERT_HEAD(tableh, entry, h_list);
}

struct hash_data *
hash_table_find(struct hash_table *htable, const char *key, size_t len)
{
	uint32_t hashv;
	struct hash_head *tableh;
	struct hash_table_entry *entry;

	hashv = hash4(key, len);
	tableh = &(htable->h_table[(hashv & (htable->h_size - 1))]);

	SLIST_FOREACH(entry, tableh, h_list) {
		if (!strcmp(entry->h_data.h_key, key))
			return (&(entry->h_data));
	}

	return (NULL);
}
