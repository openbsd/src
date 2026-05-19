/*	$OpenBSD: bus_dma.c,v 1.15 2026/05/19 13:05:47 kettenis Exp $ */

/*
 * Copyright (c) 2003-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "kstat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#if NKSTAT > 0
#include <sys/kstat.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

u_long bus_dma_high_pages;
u_long bus_dma_bounce_pages;
u_long bus_dma_bounces;

#if NKSTAT > 0
struct bus_dma_kstat_data {
	struct kstat_kv kd_bounce_pages;
	struct kstat_kv kd_bounces;
};

static const struct bus_dma_kstat_data bus_dma_kstat_tpl = {
	KSTAT_KV_INITIALIZER("bounce-pages", KSTAT_KV_T_COUNTER64),
	KSTAT_KV_INITIALIZER("bounces", KSTAT_KV_T_COUNTER64),
};

int
bus_dma_kstat_copy(struct kstat *ks, void *dst)
{
	struct bus_dma_kstat_data *kd = dst;

	*kd = bus_dma_kstat_tpl;
	kstat_kv_u64(&kd->kd_bounce_pages) = bus_dma_bounce_pages;
	kstat_kv_u64(&kd->kd_bounces) = bus_dma_bounces;

	return 0;
}
#endif

void
bus_dma_init(void)
{
	struct uvm_constraint_range high_constraint;
#if NKSTAT > 0
	struct kstat *ks;
#endif

	high_constraint.ucr_low = dma_constraint.ucr_high;
	high_constraint.ucr_high = no_constraint.ucr_high;
	if (high_constraint.ucr_low != high_constraint.ucr_high)
		high_constraint.ucr_low++;

	bus_dma_high_pages = uvm_pagecount(&high_constraint);

#if NKSTAT > 0
	ks = kstat_create("mainbus0", 0, "dma", 0, KSTAT_T_KV, 0);
	if (ks == NULL)
		return;

	ks->ks_datalen = sizeof(bus_dma_kstat_tpl);
	ks->ks_copy = bus_dma_kstat_copy;
	kstat_install(ks);
#endif
}

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct uvm_constraint_range *constraint = &no_constraint;
	int use_bounce_buffer = 0;
	struct machine_bus_dmamap *map;
	struct pglist mlist;
	struct vm_page **pg, *pgnext;
	size_t mapsize, sz, ssize;
	vaddr_t va, sva;
	void *mapstore;
	int npages, error;
	const struct kmem_dyn_mode *kd;

	if (bus_dma_high_pages > 0) {
		use_bounce_buffer = 1;
		constraint = &dma_constraint;
	}

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct machine_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));

	if (use_bounce_buffer) {
		/* this many pages plus one in case we get split */
		npages = round_page(size) / PAGE_SIZE + 1;
		if (npages < nsegments)
			npages = nsegments;
		mapsize += sizeof(struct vm_page *) * npages;
		atomic_add_long(&bus_dma_bounce_pages, npages);
	}

	if ((mapstore = malloc(mapsize, M_DEVBUF, (flags & BUS_DMA_NOWAIT) ?
	    (M_NOWAIT | M_ZERO) : (M_WAITOK | M_ZERO))) == NULL)
		return (ENOMEM);

	map = (struct machine_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->_dm_flags &= ~BUS_DMA_64BIT; /* XXX */
	if (use_bounce_buffer) {
		map->_dm_pages = (void *)&map->dm_segs[nsegments];
		map->_dm_npages = npages;
	}

	if (!use_bounce_buffer) {
		*dmamp = map;
		return (0);
	}

	sz = npages << PGSHIFT;
	kd = flags & BUS_DMA_NOWAIT ? &kd_trylock : &kd_waitok;
	va = (vaddr_t)km_alloc(sz, &kv_any, &kp_none, kd);
	if (va == 0) {
		map->_dm_npages = 0;
		free(map, M_DEVBUF, mapsize);
		return (ENOMEM);
	}

	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(sz, constraint->ucr_low,
	    constraint->ucr_high, PAGE_SIZE, 0, &mlist, nsegments,
	    (flags & BUS_DMA_NOWAIT) ? UVM_PLA_NOWAIT : UVM_PLA_WAITOK);
	if (error) {
		map->_dm_npages = 0;
		km_free((void *)va, sz, &kv_any, &kp_none);
		free(map, M_DEVBUF, mapsize);
		return (ENOMEM);
	}

	sva = va;
	ssize = sz;
	pgnext = TAILQ_FIRST(&mlist);
	for (pg = map->_dm_pages; npages--; va += PAGE_SIZE, pg++) {
		*pg = pgnext;
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(*pg),
		    PROT_READ | PROT_WRITE);
		pgnext = TAILQ_NEXT(*pg, pageq);
		memset((void *)va, 0, PAGE_SIZE);
	}
	pmap_update(pmap_kernel());
	map->_dm_pgva = sva;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	size_t mapsize;
	struct vm_page **pg;
	struct pglist mlist;

	if (map->_dm_pgva) {
		km_free((void *)map->_dm_pgva, map->_dm_npages << PGSHIFT,
		    &kv_any, &kp_none);
	}

	mapsize = sizeof(struct machine_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (map->_dm_segcnt - 1));
	mapsize += sizeof(struct vm_page *) * map->_dm_npages;

	if (map->_dm_pages) {
		TAILQ_INIT(&mlist);
		for (pg = map->_dm_pages; map->_dm_npages--; pg++) {
			TAILQ_INSERT_TAIL(&mlist, *pg, pageq);
		}
		uvm_pglistfree(&mlist);
	}

	free(map, M_DEVBUF, mapsize);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 */
int
_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf, bus_size_t buflen,
    struct proc *p, int flags)
{
	paddr_t lastaddr;
	int seg, used, error;
	int lastbounce;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	seg = 0;
	used = 0;
	lastbounce = 0;
	error = (*t->_dmamap_load_buffer)(t, map, buf, buflen, p, flags,
	    &lastaddr, &seg, &used, &lastbounce, 1);
	if (error == 0) {
		map->dm_nsegs = seg + 1;
		map->dm_mapsize = buflen;
		map->_dm_nused = used;
	}

	return (error);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m0, int flags)
{
	paddr_t lastaddr;
	int seg, used, error, first;
	int lastbounce;
	struct mbuf *m;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

#ifdef DIAGNOSTIC
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("_dmamap_load_mbuf: no packet header");
#endif

	if (m0->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	first = 1;
	seg = 0;
	used = 0;
	lastbounce = 0;
	error = 0;
	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		error = (*t->_dmamap_load_buffer)(t, map, m->m_data, m->m_len,
		    NULL, flags, &lastaddr, &seg, &used, &lastbounce, first);
		first = 0;
	}
	if (error == 0) {
		map->dm_mapsize = m0->m_pkthdr.len;
		map->dm_nsegs = seg + 1;
		map->_dm_nused = used;
	}

	return (error);
}

/*
 * Like _dmamap_load(), but for uios.
 */
int
_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio, int flags)
{
	paddr_t lastaddr;
	int seg, used, i, error, first;
	int lastbounce;
	bus_size_t minlen, resid;
	struct proc *p = NULL;
	struct iovec *iov;
	void *addr;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		p = uio->uio_procp;
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("_dmamap_load_uio: USERSPACE but no proc");
#endif
	}

	first = 1;
	seg = 0;
	used = 0;
	lastbounce = 0;
	error = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && error == 0; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		addr = (void *)iov[i].iov_base;

		error = (*t->_dmamap_load_buffer)(t, map, addr, minlen,
		    p, flags, &lastaddr, &seg, &used, &lastbounce, first);
		first = 0;

		resid -= minlen;
	}
	if (error == 0) {
		map->dm_nsegs = seg + 1;
		map->dm_mapsize = uio->uio_resid;
		map->_dm_nused = used;
	}

	return (error);
}

/*
 * Like _dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	bus_addr_t paddr, baddr, bmask, lastaddr = 0;
	bus_size_t plen, sgsize, mapsize;
	int bounce, lastbounce = 0;
	int first = 1;
	int i, seg = 0;
	int off, page = 0;
	vaddr_t pgva, vaddr;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	if (nsegs > map->_dm_segcnt || size > map->_dm_size)
		return (EINVAL);

	mapsize = size;
	bmask = ~(map->_dm_boundary - 1);

	for (i = 0; i < nsegs && size > 0; i++) {
		paddr = segs[i].ds_addr;
		vaddr = segs[i]._ds_vaddr;
		plen = MIN(segs[i].ds_len, size);

		bounce = 0;
		if (paddr + plen - 1 > dma_constraint.ucr_high)
			bounce = 1;

		while (plen > 0) {
			if (bounce) {
				if (page >= map->_dm_npages)
					return (EFBIG);

				off = paddr & PAGE_MASK;
				pgva = map->_dm_pgva + (page << PGSHIFT) + off;
				page++;
			} else {
				pgva = -1;
			}

			/*
			 * Compute the segment size, and adjust counts.
			 */
			sgsize = PAGE_SIZE - ((u_long)paddr & PGOFSET);
			if (plen < sgsize)
				sgsize = plen;

			/*
			 * Make sure we don't cross any boundaries.
			 */
			if (map->_dm_boundary > 0) {
				baddr = (paddr + map->_dm_boundary) & bmask;
				if (sgsize > (baddr - paddr))
					sgsize = (baddr - paddr);
			}

			/*
			 * Insert chunk into a segment, coalescing with
			 * previous segment if possible.
			 */
			if (first) {
				map->dm_segs[seg].ds_addr = paddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_paddr = paddr;
				map->dm_segs[seg]._ds_vaddr = vaddr;
				map->dm_segs[seg]._ds_bounce_va = pgva;
				first = 0;
			} else {
				if (paddr == lastaddr &&
				    bounce == lastbounce &&
				    (map->dm_segs[seg].ds_len + sgsize) <=
				     map->_dm_maxsegsz &&
				    (map->_dm_boundary == 0 ||
				     (map->dm_segs[seg].ds_addr & bmask) ==
				     (paddr & bmask)) &&
				    (t->_flags & BUS_DMA_COHERENT || !bounce ||
				     (map->dm_segs[seg]._ds_vaddr +
				     map->dm_segs[seg].ds_len == vaddr)))
					map->dm_segs[seg].ds_len += sgsize;
				else {
					if (++seg >= map->_dm_segcnt)
						return (EINVAL);
					map->dm_segs[seg].ds_addr = paddr;
					map->dm_segs[seg].ds_len = sgsize;
					map->dm_segs[seg]._ds_paddr = paddr;
					map->dm_segs[seg]._ds_vaddr = vaddr;
					map->dm_segs[seg]._ds_bounce_va = pgva;
				}
			}

			paddr += sgsize;
			vaddr += sgsize;
			plen -= sgsize;
			size -= sgsize;

			lastaddr = paddr;
			lastbounce = bounce;
		}
	}

	map->dm_mapsize = mapsize;
	map->dm_nsegs = seg + 1;
	map->_dm_nused = page;
	return (0);
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
void
_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	/*
	 * No resources to free; just mark the mappings as
	 * invalid.
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
	map->_dm_nused = 0;
}

static void
_dmamap_sync_segment(vaddr_t va, vsize_t len, int ops)
{
	switch (ops) {
	case BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE:
	case BUS_DMASYNC_PREREAD:
		cpu_dcache_wbinv_range(va, len);
		break;

	case BUS_DMASYNC_PREWRITE:
		cpu_dcache_wb_range(va, len);
		break;

	/*
	 * Cortex CPUs can do speculative loads so we need to clean the cache
	 * after a DMA read to deal with any speculatively loaded cache lines.
	 * Since these can't be dirty, we can just invalidate them and don't
	 * have to worry about having to write back their contents.
	 */
	case BUS_DMASYNC_POSTREAD:
	case BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE:
		membar_sync();
		cpu_dcache_inv_range(va, len);
		break;
	}
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t addr,
    bus_size_t size, int op)
{
	int coherent = 0;
	int bounce = 0;
	int nsegs;
	int curseg;

	if (t->_flags & BUS_DMA_COHERENT)
		coherent = 1;

	if (map->_dm_nused > 0)
		bounce = 1;

	/*
	 * If we're fully coherent, just make sure the write buffer is
	 * synced and return.
	 */
	if (coherent && !bounce) {
		membar_sync();
		return;
	}

	nsegs = map->dm_nsegs;
	curseg = 0;

	while (size && nsegs) {
		vaddr_t bounce_va;
		vaddr_t flush_va;
		vaddr_t vaddr;
		bus_size_t ssize;

		ssize = map->dm_segs[curseg].ds_len;
		vaddr = map->dm_segs[curseg]._ds_vaddr;
		bounce_va = map->dm_segs[curseg]._ds_bounce_va;

		if (addr != 0) {
			if (addr >= ssize) {
				addr -= ssize;
				ssize = 0;
			} else {
				vaddr += addr;
				if (bounce_va != -1)
					bounce_va += addr;
				ssize -= addr;
				addr = 0;
			}
		}
		if (ssize > size)
			ssize = size;

		if (ssize != 0) {
			if (bounce_va != -1)
				flush_va = bounce_va;
			else
				flush_va = vaddr;

			if (bounce_va != -1 && (op & BUS_DMASYNC_PREWRITE)) {
				memcpy((void *)bounce_va, (void *)vaddr,
				    ssize);
				atomic_inc_long(&bus_dma_bounces);
			}

			if ((t->_flags & BUS_DMA_COHERENT) == 0)
				_dmamap_sync_segment(flush_va, ssize, op);

			if (bounce_va != -1 && (op & BUS_DMASYNC_POSTREAD)) {
				memcpy((void *)vaddr, (void *)bounce_va,
				    ssize);
				atomic_inc_long(&bus_dma_bounces);
			}

			size -= ssize;
		}
		curseg++;
		nsegs--;
	}

	if (size != 0) {
		panic("_dmamap_sync: ran off map!");
	}
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	return _dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, dma_constraint.ucr_low,
	    dma_constraint.ucr_high);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	vm_page_t m;
	bus_addr_t addr;
	struct pglist mlist;
	int curseg;

	/*
	 * Build a list of pages to free back to the VM system.
	 */
	TAILQ_INIT(&mlist);
	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += PAGE_SIZE) {
			m = PHYS_TO_VM_PAGE(addr);
			TAILQ_INSERT_TAIL(&mlist, m, pageq);
		}
	}

	uvm_pglistfree(&mlist);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, size_t size,
    caddr_t *kvap, int flags)
{
	vaddr_t va, sva;
	size_t ssize;
	bus_addr_t addr;
	int curseg, pmap_flags, cache;
	const struct kmem_dyn_mode *kd;

	size = round_page(size);
	kd = flags & BUS_DMA_NOWAIT ? &kd_trylock : &kd_waitok;
	va = (vaddr_t)km_alloc(size, &kv_any, &kp_none, kd);
	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;

	sva = va;
	ssize = size;
	pmap_flags = PMAP_WIRED | PMAP_CANFAIL;
	cache = PMAP_CACHE_WB;
	if (((t->_flags & BUS_DMA_COHERENT) == 0 &&
	   (flags & BUS_DMA_COHERENT)) || (flags & BUS_DMA_NOCACHE))
		cache = PMAP_CACHE_CI;
	for (curseg = 0; curseg < nsegs; curseg++) {
		segs[curseg]._ds_vaddr = va;
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += NBPG, va += NBPG, size -= NBPG) {
			if (size == 0)
				panic("_dmamem_map: size botch");
			pmap_kenter_cache(va, addr,
			    PROT_READ | PROT_WRITE | pmap_flags,
			    cache);
		}
		pmap_update(pmap_kernel());
	}

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{
	km_free(kva, round_page(size), &kv_any, &kp_none);
}

/*
 * Common function for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
paddr_t
_dmamem_mmap(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, off_t off,
    int prot, int flags)
{
	int i, pmapflags = 0;

	if (flags & BUS_DMA_NOCACHE)
		pmapflags |= PMAP_NOCACHE;

	for (i = 0; i < nsegs; i++) {
#ifdef DIAGNOSTIC
		if (off & PGOFSET)
			panic("_dmamem_mmap: offset unaligned");
		if (segs[i].ds_addr & PGOFSET)
			panic("_dmamem_mmap: segment unaligned");
		if (segs[i].ds_len & PGOFSET)
			panic("_dmamem_mmap: segment size not multiple"
			    " of page size");
#endif
		if (off >= segs[i].ds_len) {
			off -= segs[i].ds_len;
			continue;
		}

		return ((segs[i].ds_addr + off) | pmapflags);
	}

	/* Page not found. */
	return (-1);
}

/**********************************************************************
 * DMA utility functions
 **********************************************************************/

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
int
_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags, paddr_t *lastaddrp,
    int *segp, int *usedp, int *lastbouncep, int first)
{
	bus_size_t sgsize;
	bus_addr_t lastaddr, baddr, bmask;
	paddr_t curaddr;
	vaddr_t pgva, vaddr = (vaddr_t)buf;
	int bounce, lastbounce;
	int seg, page, off;
	pmap_t pmap;
	struct vm_page *pg;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	page = *usedp;
	lastaddr = *lastaddrp;
	lastbounce = *lastbouncep;
	bmask  = ~(map->_dm_boundary - 1);
	if (t->_dma_mask != 0)
		bmask &= t->_dma_mask;

	for (seg = *segp; buflen > 0; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap_extract(pmap, vaddr, &curaddr) == FALSE)
			panic("_dmapmap_load_buffer: pmap_extract(%p, %lx) failed!",
			    pmap, vaddr);

		bounce = 0;
		if (curaddr > dma_constraint.ucr_high &&
		    (map->_dm_flags & BUS_DMA_64BIT) == 0)
			bounce = 1;

		if (bounce) {
			if (page >= map->_dm_npages)
				return (EFBIG);

			off = vaddr & PAGE_MASK;
			pg = map->_dm_pages[page];
			curaddr = VM_PAGE_TO_PHYS(pg) + off;
			pgva = map->_dm_pgva + (page << PGSHIFT) + off;
			page++;
		} else
			pgva = -1;

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = NBPG - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = ((bus_addr_t)curaddr + map->_dm_boundary) &
			    bmask;
			if (sgsize > (baddr - (bus_addr_t)curaddr))
				sgsize = (baddr - (bus_addr_t)curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = curaddr;
			map->dm_segs[seg].ds_len = sgsize;
			map->dm_segs[seg]._ds_paddr = curaddr;
			map->dm_segs[seg]._ds_vaddr = vaddr;
			map->dm_segs[seg]._ds_bounce_va = pgva;
			first = 0;
		} else {
			if ((bus_addr_t)curaddr == lastaddr &&
			    bounce == lastbounce &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz &&
			    (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     ((bus_addr_t)curaddr & bmask)) &&
			    (t->_flags & BUS_DMA_COHERENT || !bounce ||
			     (map->dm_segs[seg]._ds_vaddr +
			     map->dm_segs[seg].ds_len == vaddr)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt)
					break;
				map->dm_segs[seg].ds_addr = curaddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_paddr = curaddr;
				map->dm_segs[seg]._ds_vaddr = vaddr;
				map->dm_segs[seg]._ds_bounce_va = pgva;
			}
		}

		lastaddr = (bus_addr_t)curaddr + sgsize;
		lastbounce = bounce;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*usedp = page;
	*lastaddrp = lastaddr;
	*lastbouncep = lastbounce;

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		return (EFBIG);		/* XXX better return value here? */

	return (0);
}

/*
 * Allocate physical memory from the given physical address range.
 * Called by DMA-safe memory allocation methods.
 */
int
_dmamem_alloc_range(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags, paddr_t low, paddr_t high)
{
	paddr_t curaddr, lastaddr;
	vm_page_t m;
	struct pglist mlist;
	int curseg, error, plaflag;

	/* Always round the size. */
	size = round_page(size);

	/*
	 * Allocate pages from the VM system.
	 */
	plaflag = flags & BUS_DMA_NOWAIT ? UVM_PLA_NOWAIT : UVM_PLA_WAITOK;
	if (flags & BUS_DMA_ZERO)
		plaflag |= UVM_PLA_ZERO;

	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(size, low, high, alignment, boundary,
	    &mlist, nsegs, plaflag);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	m = TAILQ_FIRST(&mlist);
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = VM_PAGE_TO_PHYS(m);
	segs[curseg].ds_len = PAGE_SIZE;
	m = TAILQ_NEXT(m, pageq);

	for (; m != NULL; m = TAILQ_NEXT(m, pageq)) {
		curaddr = VM_PAGE_TO_PHYS(m);
#ifdef DIAGNOSTIC
		if (curaddr < low || curaddr >= high) {
			printf("vm_page_alloc_memory returned non-sensical"
			    " address 0x%lx\n", curaddr);
			panic("_dmamem_alloc_range");
		}
#endif
		if (curaddr == (lastaddr + PAGE_SIZE))
			segs[curseg].ds_len += PAGE_SIZE;
		else {
			curseg++;
			segs[curseg].ds_addr = curaddr;
			segs[curseg].ds_len = PAGE_SIZE;
		}
		lastaddr = curaddr;
	}

	*rsegs = curseg + 1;

	return (0);
}
