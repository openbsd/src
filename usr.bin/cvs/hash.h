/*	$OpenBSD: hash.h,v 1.1 2008/06/21 15:39:15 joris Exp $	*/
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

/*
 * This is based upon:
 *	src/lib/libc/stdlib/hcreate.c
 */

#ifndef _H_HASH
#define _H_HASH

struct hash_data {
	char	*h_key;
	void	*h_data;
};

struct hash_table_entry {
	SLIST_ENTRY(hash_table_entry)	h_list;
	struct hash_data		h_data;
};

SLIST_HEAD(hash_head, hash_table_entry);

struct hash_table {
	struct hash_head	*h_table;
	size_t			h_size;
};

#define MIN_HASH_SIZE	(1 << 4)
#define MAX_HASH_SIZE	((size_t)1 << (sizeof(size_t) * 8 - 1 - 5))

void	hash_table_init(struct hash_table *, size_t);
void	hash_table_enter(struct hash_table *, struct hash_data *);
struct hash_data *hash_table_find(struct hash_table *, const char *, size_t);

u_int32_t hash4(const char *, size_t);

extern struct hash_table created_directories;
extern struct hash_table created_cvs_directories;

#endif
