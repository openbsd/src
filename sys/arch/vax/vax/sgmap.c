/*	$OpenBSD: sgmap.c,v 1.7 2002/10/12 01:09:44 krw Exp $	*/
/* $NetBSD: sgmap.c,v 1.8 2000/06/29 07:14:34 mrg Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/sgmap.h>

void
vax_sgmap_init(t, sgmap, name, sgvabase, sgvasize, ptva, minptalign)
	bus_dma_tag_t t;
	struct vax_sgmap *sgmap;
	const char *name;
	bus_addr_t sgvabase;
	bus_size_t sgvasize;
	struct pte *ptva;
	bus_size_t minptalign;
{
	bus_dma_segment_t seg;
	size_t ptsize;
	int rseg;

	if (sgvasize & PGOFSET) {
		printf("size botch for sgmap `%s'\n", name);
		goto die;
	}

	sgmap->aps_sgvabase = sgvabase;
	sgmap->aps_sgvasize = sgvasize;

	if (ptva != NULL) {
		/*
		 * We already have a page table; this may be a system
		 * where the page table resides in bridge-resident SRAM.
		 */
		sgmap->aps_pt = ptva;
	} else {
		/*
		 * Compute the page table size and allocate it.  At minimum,
		 * this must be aligned to the page table size.  However,
		 * some platforms have more strict alignment reqirements.
		 */
		ptsize = (sgvasize / VAX_NBPG) * sizeof(struct pte);
		if (minptalign != 0) {
			if (minptalign < ptsize)
				minptalign = ptsize;
		} else
			minptalign = ptsize;
		if (bus_dmamem_alloc(t, ptsize, minptalign, 0, &seg, 1, &rseg,
		    BUS_DMA_NOWAIT)) {
			panic("unable to allocate page table for sgmap `%s'",
			    name);
			goto die;
		}
		sgmap->aps_pt = (struct pte *)(seg.ds_addr | KERNBASE);
	}

	/*
	 * Create the extent map used to manage the virtual address
	 * space.
	 */
	sgmap->aps_ex = extent_create((char *)name, sgvabase, sgvasize - 1,
	    M_DEVBUF, NULL, 0, EX_NOWAIT|EX_NOCOALESCE);
	if (sgmap->aps_ex == NULL) {
		printf("unable to create extent map for sgmap `%s'\n", name);
		goto die;
	}

	return;
 die:
	panic("vax_sgmap_init");
}

int
vax_sgmap_alloc(map, origlen, sgmap, flags)
	bus_dmamap_t map;
	bus_size_t origlen;
	struct vax_sgmap *sgmap;
	int flags;
{
	int error;
	bus_size_t len = origlen;

#ifdef DIAGNOSTIC
	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		panic("vax_sgmap_alloc: already have sgva space");
#endif

	/* If we need a spill page (for the VS4000 SCSI), make sure we 
	 * allocate enough space for an extra page.
	 */
	if (flags & VAX_BUS_DMA_SPILLPAGE) {
		len += VAX_NBPG;
	}

	map->_dm_sgvalen = vax_round_page(len);
#if 0
	printf("len %x -> %x, _dm_sgvalen %x _dm_boundary %x boundary %x -> ",
	    origlen, len, map->_dm_sgvalen, map->_dm_boundary, boundary);
#endif

	error = extent_alloc(sgmap->aps_ex, map->_dm_sgvalen, VAX_NBPG, 0,
	    0, (flags & BUS_DMA_NOWAIT) ? EX_NOWAIT : EX_WAITOK,
	    &map->_dm_sgva);
#if 0
	printf("error %d _dm_sgva %x\n", error, map->_dm_sgva);
#endif

	if (error == 0)
		map->_dm_flags |= DMAMAP_HAS_SGMAP;
	else
		map->_dm_flags &= ~DMAMAP_HAS_SGMAP;
	
	return (error);
}

void
vax_sgmap_free(map, sgmap)
	bus_dmamap_t map;
	struct vax_sgmap *sgmap;
{

#ifdef DIAGNOSTIC
	if ((map->_dm_flags & DMAMAP_HAS_SGMAP) == 0)
		panic("vax_sgmap_free: no sgva space to free");
#endif

	if (extent_free(sgmap->aps_ex, map->_dm_sgva, map->_dm_sgvalen,
	    EX_NOWAIT))
		panic("vax_sgmap_free");

	map->_dm_flags &= ~DMAMAP_HAS_SGMAP;
}

int
vax_sgmap_load(t, map, buf, buflen, p, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
	struct vax_sgmap *sgmap;
{
	vaddr_t endva, va = (vaddr_t)buf;
	paddr_t pa;
	bus_addr_t dmaoffset;
	bus_size_t dmalen;
	long *pte, *page_table = (long *)sgmap->aps_pt;
	int pteidx, error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	/*
	 * Remember the offset into the first page and the total
	 * transfer length.
	 */
	dmaoffset = ((u_long)buf) & VAX_PGOFSET;
	dmalen = buflen;


	/*
	 * Allocate the necessary virtual address space for the
	 * mapping.  Round the size, since we deal with whole pages.
	 */
	endva = vax_round_page(va + buflen);
	va = vax_trunc_page(va);
	if ((map->_dm_flags & DMAMAP_HAS_SGMAP) == 0) {
		error = vax_sgmap_alloc(map, (endva - va), sgmap, flags);
		if (error)
			return (error);
	}

	pteidx = map->_dm_sgva >> VAX_PGSHIFT;
	pte = &page_table[pteidx];

	/*
	 * Generate the DMA address.
	 */
	map->dm_segs[0].ds_addr = map->_dm_sgva + dmaoffset;
	map->dm_segs[0].ds_len = dmalen;


	map->_dm_pteidx = pteidx;
	map->_dm_ptecnt = 0;

	/*
	 * Create the bus-specific page tables.
	 * Can be done much more efficient than this.
	 */
	for (; va < endva; va += VAX_NBPG, pte++, map->_dm_ptecnt++) {
		/*
		 * Get the physical address for this segment.
		 */
		if (p != NULL)
			pmap_extract(p->p_vmspace->vm_map.pmap, va, &pa);
		else
			pa = kvtophys(va);

		/*
		 * Load the current PTE with this page.
		 */
		*pte = (pa >> VAX_PGSHIFT) | PG_V;
	}
	/* The VS4000 SCSI prefetcher doesn't like to end on a page boundary
	 * so add an extra page to quiet it down.
	 */
	if (flags & VAX_BUS_DMA_SPILLPAGE) {
		*pte = pte[-1];
		map->_dm_ptecnt++;
	}

	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	return (0);
}

int
vax_sgmap_load_mbuf(t, map, m, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
	struct vax_sgmap *sgmap;
{

	panic("vax_sgmap_load_mbuf : not implemented");
}

int
vax_sgmap_load_uio(t, map, uio, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
	struct vax_sgmap *sgmap;
{

	panic("vax_sgmap_load_uio : not implemented");
}

int
vax_sgmap_load_raw(t, map, segs, nsegs, size, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
	struct vax_sgmap *sgmap;
{

	panic("vax_sgmap_load_raw : not implemented");
}

void
vax_sgmap_unload(t, map, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct vax_sgmap *sgmap;
{
	long *pte, *page_table = (long *)sgmap->aps_pt;
	int ptecnt;

	/*
	 * Invalidate the PTEs for the mapping.
	 */
	for (ptecnt = map->_dm_ptecnt, pte = &page_table[map->_dm_pteidx];
		ptecnt-- != 0; ) {
		*pte++ = 0;
	}

	/*
	 * Free the virtual address space used by the mapping
	 * if necessary.
	 */
	if ((map->_dm_flags & BUS_DMA_ALLOCNOW) == 0)
		vax_sgmap_free(map, sgmap);
	/*
	 * Mark the mapping invalid.
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}
