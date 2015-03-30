/*	$OpenBSD: dvma.c,v 1.1 2015/03/30 20:30:22 miod Exp $	*/

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_machdep.c	8.2 (Berkeley) 9/23/93
 */

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>

/*
 * dvmamap_extent is used to manage DVMA memory.
 */
vaddr_t dvma_base, dvma_end;
struct extent *dvmamap_extent;

vaddr_t	dvma_mapin_space(struct vm_map *, vaddr_t, int, int, int);

void
dvma_init(void)
{
	dvma_base = CPU_ISSUN4M ? DVMA4M_BASE : DVMA_BASE;
	dvma_end = CPU_ISSUN4M ? DVMA4M_END : DVMA_END;
	dvmamap_extent = extent_create("dvmamap", dvma_base, dvma_end,
				       M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (dvmamap_extent == NULL)
		panic("unable to allocate extent for dvma");
}

/*
 * Wrapper for dvma_mapin_space() in kernel space,
 * so drivers need not include VM goo to get at kernel_map.
 */
caddr_t
kdvma_mapin(va, len, canwait)
	caddr_t	va;
	int	len, canwait;
{
	return (caddr_t)dvma_mapin_space(kernel_map, (vaddr_t)va, len, canwait,
	    0);
}

#if defined(SUN4M)	/* XXX iommu.c */
extern int has_iocache;
#endif

caddr_t
dvma_malloc_space(len, kaddr, flags, space)
	size_t	len;
	void	*kaddr;
	int	flags;
{
	vaddr_t	kva;
	vaddr_t	dva;

	len = round_page(len);
	kva = (vaddr_t)malloc(len, M_DEVBUF, flags);
	if (kva == 0)
		return (NULL);

#if defined(SUN4M)
	if (!has_iocache)
#endif
		kvm_uncache((caddr_t)kva, atop(len));

	*(vaddr_t *)kaddr = kva;
	dva = dvma_mapin_space(kernel_map, kva, len, (flags & M_NOWAIT) ? 0 : 1, space);
	if (dva == 0) {
		free((void *)kva, M_DEVBUF, 0);
		return (NULL);
	}
	return (caddr_t)dva;
}

void
dvma_free(dva, len, kaddr)
	caddr_t	dva;
	size_t	len;
	void	*kaddr;
{
	vaddr_t	kva = *(vaddr_t *)kaddr;

	len = round_page(len);

	dvma_mapout((vaddr_t)dva, kva, len);
	/*
	 * Even if we're freeing memory here, we can't be sure that it will
	 * be unmapped, so we must recache the memory range to avoid impact
	 * on other kernel subsystems.
	 */
#if defined(SUN4M)
	if (!has_iocache)
#endif
		kvm_recache(kaddr, atop(len));
	free((void *)kva, M_DEVBUF, 0);
}

u_long dvma_cachealign = 0;

/*
 * Map a range [va, va+len] of wired virtual addresses in the given map
 * to a valid DVMA address. On non-SRMMU systems, this establish a
 * second mapping of that range.
 */
vaddr_t
dvma_mapin_space(map, va, len, canwait, space)
	struct vm_map	*map;
	vaddr_t	va;
	int		len, canwait, space;
{
	vaddr_t	kva, tva;
	int npf, s;
	paddr_t pa;
	vaddr_t off;
	vaddr_t ova;
	int olen;
	int error;

	if (dvma_cachealign == 0)
	        dvma_cachealign = PAGE_SIZE;

	ova = va;
	olen = len;

	off = va & PAGE_MASK;
	va &= ~PAGE_MASK;
	len = round_page(len + off);
	npf = atop(len);

	s = splhigh();
	if (space & M_SPACE_D24)
		error = extent_alloc_subregion(dvmamap_extent,
		    DVMA_D24_BASE, DVMA_D24_END, len, dvma_cachealign,
		    va & (dvma_cachealign - 1), 0,
		    canwait ? EX_WAITSPACE : EX_NOWAIT, &tva);
	else
		error = extent_alloc(dvmamap_extent, len, dvma_cachealign, 
		    va & (dvma_cachealign - 1), 0,
		    canwait ? EX_WAITSPACE : EX_NOWAIT, &tva);
	splx(s);
	if (error)
		return 0;
	kva = tva;

	while (npf--) {
		if (pmap_extract(vm_map_pmap(map), va, &pa) == FALSE)
			panic("dvma_mapin: null page frame");
		pa = trunc_page(pa);

#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			iommu_enter(tva, pa);
		} else
#endif
		{
			/*
			 * pmap_enter distributes this mapping to all
			 * contexts... maybe we should avoid this extra work
			 */
#ifdef notyet
#if defined(SUN4)
			if (have_iocache)
				pa |= PG_IOC;
#endif
#endif
			pmap_kenter_pa(tva, pa | PMAP_NC,
			    PROT_READ | PROT_WRITE);
		}

		tva += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	/*
	 * XXX Only have to do this on write.
	 */
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)		/* XXX */
		cpuinfo.cache_flush((caddr_t)ova, olen);	/* XXX */

	return kva + off;
}

/*
 * Remove DVMA mapping of `va' in DVMA space at `dva'.
 */
void
dvma_mapout(dva, va, len)
	vaddr_t	dva, va;
	int		len;
{
	int s, off;
	int error;
	int dlen;

	off = (int)dva & PGOFSET;
	dva -= off;
	dlen = round_page(len + off);

#if defined(SUN4M)
	if (CPU_ISSUN4M)
		iommu_remove(dva, dlen);
	else
#endif
	{
		pmap_kremove(dva, dlen);
		pmap_update(pmap_kernel());
	}

	s = splhigh();
	error = extent_free(dvmamap_extent, dva, dlen, EX_NOWAIT);
	if (error)
		printf("dvma_mapout: extent_free failed\n");
	splx(s);

	if (CACHEINFO.c_vactype != VAC_NONE)
		cpuinfo.cache_flush((caddr_t)va, len);
}

int	dvma_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
int	dvma_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
	    struct proc *, int);
int	dvma_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *, int);
int	dvma_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
	    int, bus_size_t, int);
void	dvma_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	dvma_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t, bus_size_t,
	    int);

int	dvma_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
	    caddr_t *, int);
void	dvma_dmamem_unmap(bus_dma_tag_t, void *, size_t);

int	dvma_dmamap_load_buffer(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
	    struct proc *, int);

int	dvma_alloc(bus_dmamap_t, vaddr_t, bus_size_t, int, bus_addr_t *,
	    bus_size_t *);

struct sparc_bus_dma_tag dvma_dmatag = {
	._cookie = NULL,
	._dmamap_create = dvma_dmamap_create,
	._dmamap_destroy = _bus_dmamap_destroy,
	._dmamap_load = dvma_dmamap_load,
	._dmamap_load_mbuf = dvma_dmamap_load_mbuf,
	._dmamap_load_uio = _bus_dmamap_load_uio,
	._dmamap_load_raw = dvma_dmamap_load_raw,
	._dmamap_unload = dvma_dmamap_unload,
	._dmamap_sync = dvma_dmamap_sync,

	._dmamem_alloc = _bus_dmamem_alloc,
	._dmamem_free = _bus_dmamem_free,
	._dmamem_map = dvma_dmamem_map,
	._dmamem_unmap = _bus_dmamem_unmap,
	._dmamem_mmap = _bus_dmamem_mmap,
};

/*
 * DVMA DMA map functions.
 */
int
dvma_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	bus_dmamap_t map;
	int error;

	if ((error = _bus_dmamap_create(t, size, nsegments, maxsegsz, boundary,
	    flags, &map)) != 0)
		return (error);

	/* Enable allocations from the entire map */
	map->_dm_ex_start = DVMA_BASE;
	map->_dm_ex_end = DVMA_END;

	*dmamp = map;
	return (0);
}

/*
 * Internal routine to allocate space in the DVMA extent.
 */
int
dvma_alloc(bus_dmamap_t map, vaddr_t va, bus_size_t len, int flags,
    bus_addr_t *dvap, bus_size_t *sgsizep)
{
	bus_size_t sgsize;
	u_long align, voff, dvaddr;
	int s, error;
	int pagesz = PAGE_SIZE;

	/*
	 * Remember page offset, then truncate the buffer address to
	 * a page boundary.
	 */
	voff = va & (pagesz - 1);
	va &= -pagesz;

	if (len > map->_dm_size)
		return (EINVAL);

	sgsize = (len + voff + pagesz - 1) & -pagesz;
	align = dvma_cachealign ? dvma_cachealign : map->_dm_align;

	s = splhigh();
	error = extent_alloc_subregion(dvmamap_extent, map->_dm_ex_start,
	    map->_dm_ex_end, sgsize, align, va & (align-1), map->_dm_boundary,
	    (flags & BUS_DMA_NOWAIT) == 0 ? EX_WAITOK : EX_NOWAIT, &dvaddr);
	splx(s);
	*dvap = (bus_addr_t)dvaddr;
	*sgsizep = sgsize;
	return (error);
}

int
dvma_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	bus_size_t sgsize;
	bus_addr_t dva;
	vaddr_t va = (vaddr_t)buf;
	int pagesz = PAGE_SIZE;
	pmap_t pmap;
	int error;

	if (map->dm_nsegs >= map->_dm_segcnt)
		return (EFBIG);

	/* Allocate DVMA resources */
	if ((error = dvma_alloc(map, va, buflen, flags, &dva, &sgsize)) != 0)
		return (error);

	if ((CACHEINFO.ec_totalsize == 0))
		cpuinfo.cache_flush(buf, buflen); /* XXX - move to bus_dma_sync? */

	/*
	 * We always use just one segment.
	 */
	map->dm_segs[map->dm_nsegs].ds_addr = dva + (va & (pagesz - 1));
	map->dm_segs[map->dm_nsegs].ds_len = buflen;
	map->dm_segs[map->dm_nsegs]._ds_sgsize = sgsize;
	map->dm_nsegs++;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	for (; buflen != 0; ) {
		paddr_t pa;
		/*
		 * Get the physical address for this page.
		 */
		if (!pmap_extract(pmap, va, &pa)) {
			pmap_update(pmap_kernel());
			return (EFAULT);
		}

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = pagesz - (va & (pagesz - 1));
		if (buflen < sgsize)
			sgsize = buflen;

#ifdef notyet
#if defined(SUN4)
		if (have_iocache)
			pa |= PG_IOC;
#endif
#endif
		pmap_kenter_pa(dva, (pa & -pagesz) | PMAP_NC,
		    PROT_READ | PROT_WRITE);

		dva += pagesz;
		va += sgsize;
		buflen -= sgsize;
	}
	pmap_update(pmap_kernel());

	return (0);
}

/*
 * Prepare buffer for DMA transfer.
 */
int
dvma_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	int error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 0;

	error = dvma_dmamap_load_buffer(t, map, buf, buflen, p, flags);
	if (error)
		dvma_dmamap_unload(t, map);

	return (error);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
dvma_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m0,
    int flags)
{
	struct mbuf *m;
	int error = 0;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = m0->m_pkthdr.len;
	map->dm_nsegs = 0;

	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		error = dvma_dmamap_load_buffer(t, map, m->m_data, m->m_len,
		    NULL, flags);
	}

	if (error)
		dvma_dmamap_unload(t, map);

	return (error);
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
dvma_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	struct vm_page *m;
	paddr_t pa;
	bus_addr_t dva;
	bus_size_t sgsize;
	struct pglist *mlist;
	int pagesz = PAGE_SIZE;
	int error;

	map->dm_nsegs = 0;

	/* Allocate DVMA resources */
	if ((error = dvma_alloc(map, segs[0]._ds_va, size, flags, &dva,
	    &sgsize)) != 0)
		return (error);

	/*
	 * Note DVMA address in case bus_dmamem_map() is called later.
	 * It can then insure cache coherency by choosing a KVA that
	 * is aligned to `ds_addr'.
	 */
	segs[0].ds_addr = dva;
	segs[0].ds_len = size;

	map->dm_segs[0].ds_addr = dva;
	map->dm_segs[0].ds_len = size;
	map->dm_segs[0]._ds_sgsize = sgsize;

	/* Map physical pages into the DVMA address space */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {
		if (sgsize == 0)
			panic("dvma_dmamap_load_raw: size botch");
		pa = VM_PAGE_TO_PHYS(m);

#ifdef notyet
#if defined(SUN4)
		if (have_iocache)
			pa |= PG_IOC;
#endif
#endif
		pmap_kenter_pa(dva, (pa & -pagesz) | PMAP_NC,
		    PROT_READ | PROT_WRITE);

		dva += pagesz;
		sgsize -= pagesz;
	}
	pmap_update(pmap_kernel());

	map->dm_nsegs = 1;
	map->dm_mapsize = size;

	return (0);
}

/*
 * Unload a DVMA DMA map.
 */
void
dvma_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	bus_dma_segment_t *segs = map->dm_segs;
	int nsegs = map->dm_nsegs;
	bus_addr_t dva;
	bus_size_t len;
	int i, s, error;

	for (i = 0; i < nsegs; i++) {
		dva = segs[i].ds_addr & -PAGE_SIZE;
		len = segs[i]._ds_sgsize;

		pmap_kremove(dva, len);
		s = splhigh();
		error = extent_free(dvmamap_extent, dva, len, EX_NOWAIT);
		splx(s);
		if (error != 0)
			printf("warning: %ld of DVMA space lost\n", (long)len);
	}
	pmap_update(pmap_kernel());

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

/*
 * DMA map synchronization.
 */
void
dvma_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t offset,
    bus_size_t len, int ops)
{

	/*
	 * XXX Should flush CPU write buffers.
	 */
}

/*
 * Map DMA-safe memory.
 */
int
dvma_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	struct vm_page *m;
	vaddr_t va;
	bus_addr_t addr;
	struct pglist *mlist;
	u_long align;
	int pagesz = PAGE_SIZE;
	const struct kmem_dyn_mode *kd;

	if (nsegs != 1)
		panic("dvma_dmamem_map: nsegs = %d", nsegs);

	align = dvma_cachealign ? dvma_cachealign : pagesz;

	size = round_page(size);

	kd = flags & BUS_DMA_NOWAIT ? &kd_trylock : &kd_waitok;
	va = (vaddr_t)km_alloc(size, &kv_any, &kp_none, kd);
	if (va == 0)
		return (ENOMEM);

	segs[0]._ds_va = va;
	*kvap = (void *)va;

	/*
	 * Map the pages allocated in _bus_dmamem_alloc() to the
	 * kernel virtual address space.
	 */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {

		if (size == 0)
			panic("dvma_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_kenter_pa(va, addr | PMAP_NC, PROT_READ | PROT_WRITE);

		va += pagesz;
		size -= pagesz;
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
dvma_dmamem_unmap(bus_dma_tag_t t, void *kva, size_t size)
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PAGE_MASK)
		panic("dvma_dmamem_unmap");
#endif

	km_free(kva, round_page(size), &kv_any, &kp_none);
}
