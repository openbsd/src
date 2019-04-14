/*	$OpenBSD: drm_cache.c,v 1.7 2019/04/14 10:14:51 jsg Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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

#include <drm/drmP.h>

#if defined(__i386__) || defined(__amd64__)
static void
drm_clflush_page(struct vm_page *page)
{
	void *addr;

	if (page == NULL)
		return;

	addr = kmap_atomic(page);
	pmap_flush_cache((vaddr_t)addr, PAGE_SIZE);
	kunmap_atomic(addr);
}

void
drm_clflush_pages(struct vm_page *pages[], unsigned long num_pages)
{
	unsigned long i;

	for (i = 0; i < num_pages; i++)
		drm_clflush_page(*pages++);
}

void
drm_clflush_sg(struct sg_table *st)
{
	struct sg_page_iter sg_iter;

	for_each_sg_page(st->sgl, &sg_iter, st->nents, 0)
		drm_clflush_page(sg_page_iter_page(&sg_iter));
}

void
drm_clflush_virt_range(void *addr, unsigned long length)
{
	pmap_flush_cache((vaddr_t)addr, length);
}
#else
void
drm_clflush_pages(struct vm_page *pages[], unsigned long num_pages)
{
	STUB();
}
#endif
