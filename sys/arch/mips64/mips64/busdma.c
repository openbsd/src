/*	$OpenBSD: busdma.c,v 1.7 2004/12/25 23:02:24 miod Exp $ */

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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <mips64/archtype.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>

#include <machine/bus.h>

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct machine_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

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
	if ((mapstore = malloc(mapsize, M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	bzero(mapstore, mapsize);
	map = (struct machine_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);

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
	free(map, M_DEVBUF);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 */
int
_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	caddr_t vaddr = buf;
	int first, seg;
	pmap_t pmap;
	bus_size_t saved_buflen;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	lastaddr = ~0;		/* XXX gcc */
	bmask  = ~(map->_dm_boundary - 1);

	saved_buflen = buflen;
	for (first = 1, seg = 0; buflen > 0; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap_extract(pmap, (vaddr_t)vaddr, (paddr_t *)&curaddr) ==
		    FALSE)
			panic("_dmapmap_load: pmap_extract(%x, %x) failed!",
			    pmap, vaddr);

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
			baddr = (curaddr + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = curaddr + t->dma_offs;
			map->dm_segs[seg].ds_len = sgsize;
			map->dm_segs[seg].ds_vaddr = (vaddr_t)vaddr;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz &&
			     (map->_dm_boundary == 0 ||
			     ((map->dm_segs[seg].ds_addr - t->dma_offs) & bmask) ==
			     (curaddr & bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt)
					break;
				map->dm_segs[seg].ds_addr = curaddr + t->dma_offs;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg].ds_vaddr = (vaddr_t)vaddr;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		return (EFBIG);		/* XXX better return value here? */

	map->dm_nsegs = seg + 1;
	map->dm_mapsize = saved_buflen;
	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	int i;
	size_t len;

	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

	i = 0;
	len = 0;
	while (m) {
		vaddr_t vaddr = mtod(m, vaddr_t);
		long buflen = (long)m->m_len;

		len += buflen;
		while (buflen > 0 && i < map->_dm_segcnt) {
			paddr_t pa;
			long incr;

			incr = min(buflen, NBPG);
			buflen -= incr;
			if (pmap_extract(pmap_kernel(), vaddr, &pa) == FALSE)
				panic("_dmamap_load_mbuf: pmap_extract(%x, %x) failed!",
				    pmap_kernel(), vaddr);
			pa += t->dma_offs;

			if (i > 0 && pa == (map->dm_segs[i-1].ds_addr + map->dm_segs[i-1].ds_len)
			    && ((map->dm_segs[i-1].ds_len + incr) < map->_dm_maxsegsz)) {
				/* Hey, waddyaknow, they're contiguous */
				map->dm_segs[i-1].ds_len += incr;
				continue;
			}
			map->dm_segs[i].ds_addr = pa;
			map->dm_segs[i].ds_vaddr = vaddr;
			map->dm_segs[i].ds_len = incr;
			i++;
			vaddr += incr;
		}
		m = m->m_next;
		if (m && i >= map->_dm_segcnt) {
			/* Exceeded the size of our dmamap */
			return E2BIG;
		}
	}
	map->dm_nsegs = i;
	map->dm_mapsize = len;
	return (0);
}

/*
 * Like _dmamap_load(), but for uios.
 */
int
_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	return (EOPNOTSUPP);
}

/*
 * Like _dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	if (nsegs > map->_dm_segcnt || size > map->_dm_size)
		return (EINVAL);

	/*
	 * Make sure we don't cross any boundaries.
	 */
	if (map->_dm_boundary) {
		bus_addr_t bmask = ~(map->_dm_boundary - 1);
		int i;

		for (i = 0; i < nsegs; i++) {
			if (segs[i].ds_len > map->_dm_maxsegsz)
				return (EINVAL);
			if ((segs[i].ds_addr & bmask) !=
			    ((segs[i].ds_addr + segs[i].ds_len - 1) & bmask))
				return (EINVAL);
		}
	}

	bcopy(segs, map->dm_segs, nsegs * sizeof(*segs));
	map->dm_nsegs = nsegs;
	map->dm_mapsize = size;
	return (0);
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
void
_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	/*
	 * No resources to free; just mark the mappings as
	 * invalid.
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_dmamap_sync(t, map, addr, size, op)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t addr;
	bus_size_t size;
	bus_dmasync_op_t op;
{
#define SYNC_R 0
#define SYNC_W 1
#define SYNC_X 2
	int nsegs;
	int curseg;

	nsegs = map->dm_nsegs;
	curseg = 0;

#ifdef DEBUG_BUSDMASYNC
	printf("dmasync %p:%p:%p:", map, addr, size);
	if (op & BUS_DMASYNC_PREWRITE) printf("PRW ");
	if (op & BUS_DMASYNC_PREREAD) printf("PRR ");
	if (op & BUS_DMASYNC_POSTWRITE) printf("POW ");
	if (op & BUS_DMASYNC_POSTREAD) printf("POR ");
	printf("\n");
#endif

	while (size && nsegs) {
		bus_addr_t vaddr;
		bus_size_t ssize;

		ssize = map->dm_segs[curseg].ds_len;
		vaddr = map->dm_segs[curseg].ds_vaddr;

		if (addr > 0) {
			if (addr > ssize) {
				addr -= ssize;
				ssize = 0;
			} else {
				vaddr += addr;
				ssize -= addr;
			}
		}
		if (ssize > size) {
			ssize = size;
		}

		if (ssize) {
// #define DEBUG_BUSDMASYNC_FRAG
#ifdef DEBUG_BUSDMASYNC_FRAG
	printf(" syncing %p:%p ", vaddr, ssize);
	if (op & BUS_DMASYNC_PREWRITE) printf("PRW ");
	if (op & BUS_DMASYNC_PREREAD) printf("PRR ");
	if (op & BUS_DMASYNC_POSTWRITE) printf("POW ");
	if (op & BUS_DMASYNC_POSTREAD) printf("POR ");
	printf("\n");
#endif
			/*
			 *  If only PREWRITE is requested, writeback and
			 *  invalidate. PREWRITE with PREREAD writebacks
			 *  and invalidates *all* cache levels.
			 *  Otherwise, just invalidate.
			 *  POSTREAD and POSTWRITE are no-ops since
			 *  we are not bouncing data.
			 */
			if (op & BUS_DMASYNC_PREWRITE) {
				if (op & BUS_DMASYNC_PREREAD)
					Mips_IOSyncDCache(vaddr, ssize, SYNC_X);
				else
					Mips_IOSyncDCache(vaddr, ssize, SYNC_W);
			} else if (op & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_POSTREAD)) {
				Mips_IOSyncDCache(vaddr, ssize, SYNC_R);
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
_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	return (_dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, 0, 0xf0000000));
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
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
			m = PHYS_TO_VM_PAGE(addr - t->dma_offs);
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
_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vaddr_t va;
	bus_addr_t addr;
	int curseg;

	size = round_page(size);
	va = uvm_km_valloc(kmem_map, size);
	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;

	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += NBPG, va += NBPG, size -= NBPG) {
			if (size == 0)
				panic("_dmamem_map: size botch");
			pmap_enter(pmap_kernel(), va, addr - t->dma_offs,
			    VM_PROT_READ | VM_PROT_WRITE,
			    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
			segs[curseg].ds_vaddr = va;

			if (flags & BUS_DMA_COHERENT &&
			    sys_config.system_type == SGI_O2) 
				pmap_page_cache(PHYS_TO_VM_PAGE(addr - t->dma_offs), PV_UNCACHED);
		}
	}

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_dmamem_unmap");
#endif

	size = round_page(size);
	uvm_km_free(kmem_map, (vaddr_t)kva, size);
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
paddr_t
_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	off_t off;
	int prot, flags;
{
	int i;

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

		return (mips_btop((caddr_t)segs[i].ds_addr + off - t->dma_offs));
	}

	/* Page not found. */
	return (-1);
}

/**********************************************************************
 * DMA utility functions
 **********************************************************************/

/*
 * Allocate physical memory from the given physical address range.
 * Called by DMA-safe memory allocation methods.
 */
int
_dmamem_alloc_range(t, size, alignment, boundary, segs, nsegs, rsegs,
    flags, low, high)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
	vaddr_t low;
	vaddr_t high;
{
	vaddr_t curaddr, lastaddr;
	vm_page_t m;
	struct pglist mlist;
	int curseg, error;

	/* Always round the size. */
	size = round_page(size);

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(size, low, high,
	    alignment, boundary, &mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	m = TAILQ_FIRST(&mlist);
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = VM_PAGE_TO_PHYS(m);
	segs[curseg].ds_addr += t->dma_offs;
	segs[curseg].ds_len = PAGE_SIZE;
	m = TAILQ_NEXT(m, pageq);

	for (; m != TAILQ_END(&mlist); m = TAILQ_NEXT(m, pageq)) {
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
			segs[curseg].ds_addr = curaddr + t->dma_offs;
			segs[curseg].ds_len = PAGE_SIZE;
		}
		lastaddr = curaddr;
	}

	*rsegs = curseg + 1;

	return (0);
}

