/*	$OpenBSD: highmem.h,v 1.4 2021/07/28 13:28:05 kettenis Exp $	*/
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

#include <sys/param.h>
#include <uvm/uvm_extern.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

void	*kmap(struct vm_page *);
void	kunmap_va(void *addr);

#define kmap_to_page(ptr)	(ptr)

void	*kmap_atomic_prot(struct vm_page *, pgprot_t);
void	kunmap_atomic(void *);

static inline void *
kmap_atomic(struct vm_page *pg)
{
	return kmap_atomic_prot(pg, PAGE_KERNEL);
}

#endif
