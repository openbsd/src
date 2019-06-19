/*	$OpenBSD: highmem.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
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

#ifndef _LINUX_HIGHMEM_H
#define _LINUX_HIGHMEM_H

#include <uvm/uvm_extern.h>
#include <linux/uaccess.h>

void	*kmap(struct vm_page *);
void	 kunmap(void *addr);

#define kmap_to_page(ptr)	(ptr)

#if defined(__i386__) || defined(__amd64__)

static inline void *
kmap_atomic(struct vm_page *pg)
{
	vaddr_t va;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	extern vaddr_t pmap_tmpmap_pa(paddr_t);
	va = pmap_tmpmap_pa(VM_PAGE_TO_PHYS(pg));
#endif
	return (void *)va;
}

static inline void
kunmap_atomic(void *addr)
{
#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct((vaddr_t)addr);
#else
	extern void pmap_tmpunmap_pa(void);
	pmap_tmpunmap_pa();
#endif
}

#endif /* defined(__i386__) || defined(__amd64__) */

#endif
