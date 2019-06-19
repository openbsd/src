/*	$OpenBSD: set_memory.h,v 1.1 2019/04/14 10:14:52 jsg Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
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

#ifndef _ASM_SET_MEMORY_H
#define _ASM_SET_MEMORY_H

#include <sys/atomic.h>
#include <machine/pmap.h>

#if defined(__amd64__) || defined(__i386__)

static inline int
set_pages_array_wb(struct vm_page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		atomic_clearbits_int(&pages[i]->pg_flags, PG_PMAP_WC);

	return 0;
}

static inline int
set_pages_array_wc(struct vm_page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		atomic_setbits_int(&pages[i]->pg_flags, PG_PMAP_WC);

	return 0;
}

static inline int
set_pages_array_uc(struct vm_page **pages, int addrinarray)
{
	/* XXX */
	return 0;
}

static inline int
set_pages_wb(struct vm_page *page, int numpages)
{
	KASSERT(numpages == 1);
	atomic_clearbits_int(&page->pg_flags, PG_PMAP_WC);
	return 0;
}

static inline int
set_pages_uc(struct vm_page *page, int numpages)
{
	/* XXX */
	return 0;
}

#endif /* defined(__amd64__) || defined(__i386__) */

#endif
