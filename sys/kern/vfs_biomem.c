/*	$OpenBSD: vfs_biomem.c,v 1.15 2011/04/02 16:47:17 beck Exp $ */
/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
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
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/pool.h>
#include <sys/proc.h>		/* XXX for atomic */
#include <sys/mount.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

vaddr_t buf_kva_start, buf_kva_end;
int buf_needva;
TAILQ_HEAD(,buf) buf_valist;

int buf_nkvmsleep;

extern struct bcachestats bcstats;

/*
 * Pages are allocated from a uvm object (we only use it for page storage,
 * all pages are wired). Since every buffer contains a contiguous range of
 * pages, reusing the pages could be very painful. Fortunately voff_t is
 * 64 bits, so we can just increment buf_page_offset all the time and ignore
 * wraparound. Even if you reuse 4GB worth of buffers every second
 * you'll still run out of time_t faster than buffers.
 *
 * XXX - the spl locking in here is extreme paranoia right now until I figure
 *       it all out.
 */
voff_t buf_page_offset;
struct uvm_object *buf_object, buf_object_store;

vaddr_t buf_unmap(struct buf *);

void
buf_mem_init(vsize_t size)
{
	TAILQ_INIT(&buf_valist);

	buf_kva_start = vm_map_min(kernel_map);
	if (uvm_map(kernel_map, &buf_kva_start, size, NULL,
	    UVM_UNKNOWN_OFFSET, PAGE_SIZE, UVM_MAPFLAG(UVM_PROT_NONE,
	    UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_NORMAL, 0)))
		panic("bufinit: can't reserve VM for buffers");
	buf_kva_end = buf_kva_start + size;

	buf_object = &buf_object_store;

	uvm_objinit(buf_object, NULL, 1);
}

/*
 * buf_acquire and buf_release manage the kvm mappings of buffers.
 */
void
buf_acquire(struct buf *bp)
{
	int s;

	KASSERT((bp->b_flags & B_BUSY) == 0);

	s = splbio();
	/*
	 * Busy before waiting for kvm.
	 */
	SET(bp->b_flags, B_BUSY);
	buf_map(bp);

	splx(s);
}

/*
 * Busy a buffer, but don't map it.
 * If it has a mapping, we keep it, but we also keep the mapping on
 * the list since we assume that it won't be used anymore.
 */
void
buf_acquire_unmapped(struct buf *bp)
{
	int s;

	s = splbio();
	SET(bp->b_flags, B_BUSY|B_NOTMAPPED);
	splx(s);
}

void
buf_map(struct buf *bp)
{
	vaddr_t va;

	splassert(IPL_BIO);

	if (bp->b_data == NULL) {
		unsigned long i;

		/*
		 * First, just use the pre-allocated space until we run out.
		 */
		if (buf_kva_start < buf_kva_end) {
			va = buf_kva_start;
			buf_kva_start += MAXPHYS;
		} else {
			struct buf *vbp;

			/*
			 * Find some buffer we can steal the space from.
			 */
			while ((vbp = TAILQ_FIRST(&buf_valist)) == NULL) {
				buf_needva++;
				buf_nkvmsleep++;
				tsleep(&buf_needva, PRIBIO, "buf_needva", 0);
			}
			va = buf_unmap(vbp);
		}

		for (i = 0; i < atop(bp->b_bufsize); i++) {
			struct vm_page *pg = uvm_pagelookup(bp->b_pobj,
			    bp->b_poffs + ptoa(i));

			KASSERT(pg != NULL);

			pmap_kenter_pa(va + ptoa(i), VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ|VM_PROT_WRITE);
			pmap_update(pmap_kernel());
		}
		bp->b_data = (caddr_t)va;
	} else {
		TAILQ_REMOVE(&buf_valist, bp, b_valist);
	}

	bcstats.busymapped++;

	CLR(bp->b_flags, B_NOTMAPPED);
}

void
buf_release(struct buf *bp)
{
	int s;

	KASSERT(bp->b_flags & B_BUSY);
	KASSERT((bp->b_data != NULL) || (bp->b_flags & B_NOTMAPPED));

	s = splbio();
	if (bp->b_data) {
		bcstats.busymapped--;
		TAILQ_INSERT_TAIL(&buf_valist, bp, b_valist);
		if (buf_needva) {
			buf_needva--;
			wakeup_one(&buf_needva);
		}
	}
	CLR(bp->b_flags, B_BUSY|B_NOTMAPPED);
	splx(s);
}

/*
 * Deallocate all memory resources for this buffer. We need to be careful
 * to not drop kvm since we have no way to reclaim it. So, if the buffer
 * has kvm, we need to free it later. We put it on the front of the
 * freelist just so it gets picked up faster.
 *
 * Also, lots of assertions count on bp->b_data being NULL, so we
 * set it temporarily to NULL.
 *
 * Return non-zero if we take care of the freeing later.
 */
int
buf_dealloc_mem(struct buf *bp)
{
	caddr_t data;
	int s;

	s = splbio();

	data = bp->b_data;
	bp->b_data = NULL;

	if (data) {
		if (bp->b_flags & B_BUSY)
			bcstats.busymapped--;
		pmap_kremove((vaddr_t)data, bp->b_bufsize);
		pmap_update(pmap_kernel());
	}

	if (bp->b_pobj)
		buf_free_pages(bp);

	if (data == NULL) {
		splx(s);
		return (0);
	}

	bp->b_data = data;
	if (!(bp->b_flags & B_BUSY))		/* XXX - need better test */
		TAILQ_REMOVE(&buf_valist, bp, b_valist);
	else
		CLR(bp->b_flags, B_BUSY);
	SET(bp->b_flags, B_RELEASED);
	TAILQ_INSERT_HEAD(&buf_valist, bp, b_valist);

	splx(s);

	return (1);
}

void
buf_shrink_mem(struct buf *bp, vsize_t newsize)
{
	vaddr_t va = (vaddr_t)bp->b_data;

	if (newsize < bp->b_bufsize) {
		pmap_kremove(va + newsize, bp->b_bufsize - newsize);
		pmap_update(pmap_kernel());
		bp->b_bufsize = newsize;
	}
}

vaddr_t
buf_unmap(struct buf *bp)
{
	vaddr_t va;
	int s;

	KASSERT((bp->b_flags & B_BUSY) == 0);
	KASSERT(bp->b_data != NULL);

	s = splbio();
	TAILQ_REMOVE(&buf_valist, bp, b_valist);
	va = (vaddr_t)bp->b_data;
	bp->b_data = 0;
	pmap_kremove(va, bp->b_bufsize);
	pmap_update(pmap_kernel());

	if (bp->b_flags & B_RELEASED)
		pool_put(&bufpool, bp);

	splx(s);

	return (va);
}

/* Always allocates in dma-reachable memory */
void
buf_alloc_pages(struct buf *bp, vsize_t size)
{
	voff_t offs;
	int s;

	KASSERT(size == round_page(size));
	KASSERT(bp->b_pobj == NULL);
	KASSERT(bp->b_data == NULL);

	s = splbio();

	offs = buf_page_offset;
	buf_page_offset += size;

	KASSERT(buf_page_offset > 0);

	uvm_pagealloc_multi(buf_object, offs, size, UVM_PLA_WAITOK);
	bcstats.numbufpages += atop(size);
	bp->b_pobj = buf_object;
	bp->b_poffs = offs;
	bp->b_bufsize = size;
	splx(s);
}

void
buf_free_pages(struct buf *bp)
{
	struct uvm_object *uobj = bp->b_pobj;
	struct vm_page *pg;
	voff_t off, i;
	int s;

	KASSERT(bp->b_data == NULL);
	KASSERT(uobj != NULL);

	s = splbio();

	off = bp->b_poffs;
	bp->b_pobj = NULL;
	bp->b_poffs = 0;

	for (i = 0; i < atop(bp->b_bufsize); i++) {
		pg = uvm_pagelookup(uobj, off + ptoa(i));
		KASSERT(pg != NULL);
		KASSERT(pg->wire_count == 1);
		pg->wire_count = 0;
		uvm_pagefree(pg);
		bcstats.numbufpages--;
	}
	splx(s);
}

/*
 * XXX - it might make sense to make a buf_realloc_pages to avoid
 *       bouncing through the free list all the time.
 */
