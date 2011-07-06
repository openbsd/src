/*	$OpenBSD: hiballoc.h,v 1.4 2011/07/06 19:42:49 ariane Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@stack.nl>
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

#ifndef _SYS_HIBALLOC_H_
#define _SYS_HIBALLOC_H_

#include <sys/types.h>
#include <sys/tree.h>

struct hiballoc_entry;

/*
 * Hibernate allocator.
 *
 * Allocator operates from an arena, that is pre-allocated by the caller.
 */
struct hiballoc_arena
{
	RB_HEAD(hiballoc_addr, hiballoc_entry)
				hib_addrs;
};

void	*hib_alloc(struct hiballoc_arena*, size_t);
void	 hib_free(struct hiballoc_arena*, void*);
int	 hiballoc_init(struct hiballoc_arena*, void*, size_t len);

#endif /* _SYS_HIBALLOC_H_ */
