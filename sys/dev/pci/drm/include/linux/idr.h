/*	$OpenBSD: idr.h,v 1.2 2019/05/13 16:23:15 jsg Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
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

#ifndef _LINUX_IDR_H
#define _LINUX_IDR_H

#include <sys/tree.h>

struct idr_entry {
	SPLAY_ENTRY(idr_entry) entry;
	int id;
	void *ptr;
};

struct idr {
	SPLAY_HEAD(idr_tree, idr_entry) tree;
};

void idr_init(struct idr *);
void idr_preload(unsigned int);
int idr_alloc(struct idr *, void *, int, int, unsigned int);
#define idr_preload_end()
void *idr_find(struct idr *, int);
void *idr_replace(struct idr *, void *ptr, int);
void *idr_remove(struct idr *, int);
void idr_destroy(struct idr *);
int idr_for_each(struct idr *, int (*)(int, void *, void *), void *);
void *idr_get_next(struct idr *, int *);
#define idr_init_base(idr, base)	idr_init(idr)

#define idr_for_each_entry(idp, entry, id) \
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; id++)

static inline bool
idr_is_empty(const struct idr *idr)
{
	return SPLAY_EMPTY(&idr->tree);
}

struct ida {
	int counter;
};

#define DEFINE_IDA(name)	\
	struct ida name = {	\
		.counter = 0	\
	}

void ida_init(struct ida *);
void ida_destroy(struct ida *);
int ida_simple_get(struct ida *, unsigned int, unsigned nt, int);
void ida_remove(struct ida *, int);
void ida_simple_remove(struct ida *, int);

#endif
