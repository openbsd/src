/* $OpenBSD: sgmap_typedep.c,v 1.2 2001/06/08 08:08:40 art Exp $ */
/* $NetBSD: sgmap_typedep.c,v 1.13 1999/07/08 18:05:23 thorpej Exp $ */

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

#ifdef SGMAP_LOG

#ifndef SGMAP_LOGSIZE
#define	SGMAP_LOGSIZE	4096
#endif

struct sgmap_log_entry	__C(SGMAP_TYPE,_log)[SGMAP_LOGSIZE];
int			__C(SGMAP_TYPE,_log_next);
int			__C(SGMAP_TYPE,_log_last);
u_long			__C(SGMAP_TYPE,_log_loads);
u_long			__C(SGMAP_TYPE,_log_unloads);

#endif /* SGMAP_LOG */

#ifdef SGMAP_DEBUG
int			__C(SGMAP_TYPE,_debug) = 0;
#endif

SGMAP_PTE_TYPE		__C(SGMAP_TYPE,_prefetch_spill_page_pte);

void
__C(SGMAP_TYPE,_init_spill_page_pte)()
{

	__C(SGMAP_TYPE,_prefetch_spill_page_pte) =
	    (alpha_sgmap_prefetch_spill_page_pa >>
	     SGPTE_PGADDR_SHIFT) | SGPTE_VALID;
}

int
__C(SGMAP_TYPE,_load)(t, map, buf, buflen, p, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
	struct alpha_sgmap *sgmap;
{
	vaddr_t endva, va = (vaddr_t)buf;
	paddr_t pa;
	bus_addr_t dmaoffset;
	bus_size_t dmalen;
	SGMAP_PTE_TYPE *pte, *page_table = sgmap->aps_pt;
	int pteidx, error;
#ifdef SGMAP_LOG
	struct sgmap_log_entry sl;
#endif

	/*
	 * Initialize the spill page PTE if that hasn't already been done.
	 */
	if (__C(SGMAP_TYPE,_prefetch_spill_page_pte) == 0)
		__C(SGMAP_TYPE,_init_spill_page_pte)();

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
	dmaoffset = ((u_long)buf) & PGOFSET;
	dmalen = buflen;

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug)) {
		printf("sgmap_load: ----- buf = %p -----\n", buf);
		printf("sgmap_load: dmaoffset = 0x%lx, dmalen = 0x%lx\n",
		    dmaoffset, dmalen);
	}
#endif

#ifdef SGMAP_LOG
	if (panicstr == NULL) {
		sl.sl_op = 1;
		sl.sl_sgmap = sgmap;
		sl.sl_origbuf = buf;
		sl.sl_pgoffset = dmaoffset;
		sl.sl_origlen = dmalen;
	}
#endif

	/*
	 * Allocate the necessary virtual address space for the
	 * mapping.  Round the size, since we deal with whole pages.
	 *
	 * alpha_sgmap_alloc will deal with the appropriate spill page
	 * allocations.
	 *
	 */
	endva = round_page(va + buflen);
	va = trunc_page(va);
	if ((map->_dm_flags & DMAMAP_HAS_SGMAP) == 0) {
		error = alpha_sgmap_alloc(map, (endva - va), sgmap, flags);
		if (error)
			return (error);
	}

	pteidx = map->_dm_sgva >> PGSHIFT;
	pte = &page_table[pteidx * SGMAP_PTE_SPACING];

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug))
		printf("sgmap_load: sgva = 0x%lx, pteidx = %d, "
		    "pte = %p (pt = %p)\n", map->_dm_sgva, pteidx, pte,
		    page_table);
#endif

	/*
	 * Generate the DMA address.
	 */
	map->dm_segs[0].ds_addr = sgmap->aps_wbase |
	    (pteidx << SGMAP_ADDR_PTEIDX_SHIFT) | dmaoffset;
	map->dm_segs[0].ds_len = dmalen;

#ifdef SGMAP_LOG
	if (panicstr == NULL) {
		sl.sl_sgva = map->_dm_sgva;
		sl.sl_dmaaddr = map->dm_segs[0].ds_addr;
	}
#endif

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug))
		printf("sgmap_load: wbase = 0x%lx, vpage = 0x%x, "
		    "dma addr = 0x%lx\n", sgmap->aps_wbase,
		    (pteidx << SGMAP_ADDR_PTEIDX_SHIFT),
		    map->dm_segs[0].ds_addr);
#endif

	map->_dm_pteidx = pteidx;
	map->_dm_ptecnt = 0;

	for (; va < endva; va += NBPG, pteidx++,
		pte = &page_table[pteidx * SGMAP_PTE_SPACING],
		map->_dm_ptecnt++) {
		/*
		 * Get the physical address for this segment.
		 */
		if (p != NULL)
			pmap_extract(p->p_vmspace->vm_map.pmap, va, &pa);
		else
			pa = vtophys(va);

		/*
		 * Load the current PTE with this page.
		 */
		*pte = (pa >> SGPTE_PGADDR_SHIFT) | SGPTE_VALID;
#ifdef SGMAP_DEBUG
		if (__C(SGMAP_TYPE,_debug))
			printf("sgmap_load:     pa = 0x%lx, pte = %p, "
			    "*pte = 0x%lx\n", pa, pte, (u_long)(*pte));
#endif
	}

	/*
	 * ...and the prefetch-spill page.
	 */
	*pte = __C(SGMAP_TYPE,_prefetch_spill_page_pte);
	map->_dm_ptecnt++;
#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug)) {
		printf("sgmap_load:     spill page, pte = %p, *pte = 0x%lx\n",
		    pte, *pte);
		printf("sgmap_load:     pte count = %d\n", map->_dm_ptecnt);
	}
#endif

	alpha_mb();

#ifdef SGMAP_LOG
	if (panicstr == NULL) {
		sl.sl_ptecnt = map->_dm_ptecnt;
		bcopy(&sl, &__C(SGMAP_TYPE,_log)[__C(SGMAP_TYPE,_log_next)],
		    sizeof(sl));
		__C(SGMAP_TYPE,_log_last) = __C(SGMAP_TYPE,_log_next);
		if (++__C(SGMAP_TYPE,_log_next) == SGMAP_LOGSIZE)
			__C(SGMAP_TYPE,_log_next) = 0;
		__C(SGMAP_TYPE,_log_loads)++;
	}
#endif

#if defined(SGMAP_DEBUG) && defined(DDB)
	if (__C(SGMAP_TYPE,_debug) > 1)
		Debugger();
#endif
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	return (0);
}

int
__C(SGMAP_TYPE,_load_mbuf)(t, map, m, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
	struct alpha_sgmap *sgmap;
{

	panic(__S(__C(SGMAP_TYPE,_load_mbuf)) ": not implemented");
}

int
__C(SGMAP_TYPE,_load_uio)(t, map, uio, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
	struct alpha_sgmap *sgmap;
{

	panic(__S(__C(SGMAP_TYPE,_load_uio)) ": not implemented");
}

int
__C(SGMAP_TYPE,_load_raw)(t, map, segs, nsegs, size, flags, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
	struct alpha_sgmap *sgmap;
{

	panic(__S(__C(SGMAP_TYPE,_load_raw)) ": not implemented");
}

void
__C(SGMAP_TYPE,_unload)(t, map, sgmap)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct alpha_sgmap *sgmap;
{
	SGMAP_PTE_TYPE *pte, *page_table = sgmap->aps_pt;
	int ptecnt, pteidx;
#ifdef SGMAP_LOG
	struct sgmap_log_entry *sl;

	if (panicstr == NULL) {
		sl = &__C(SGMAP_TYPE,_log)[__C(SGMAP_TYPE,_log_next)];

		bzero(sl, sizeof(*sl));
		sl->sl_op = 0;
		sl->sl_sgmap = sgmap;
		sl->sl_sgva = map->_dm_sgva;
		sl->sl_dmaaddr = map->dm_segs[0].ds_addr;

		__C(SGMAP_TYPE,_log_last) = __C(SGMAP_TYPE,_log_next);
		if (++__C(SGMAP_TYPE,_log_next) == SGMAP_LOGSIZE)
			__C(SGMAP_TYPE,_log_next) = 0;
		__C(SGMAP_TYPE,_log_unloads)++;
	}
#endif

	/*
	 * Invalidate the PTEs for the mapping.
	 */
	for (ptecnt = map->_dm_ptecnt, pteidx = map->_dm_pteidx,
		pte = &page_table[pteidx * SGMAP_PTE_SPACING];
		ptecnt != 0;
		ptecnt--, pteidx++,
		pte = &page_table[pteidx * SGMAP_PTE_SPACING]) {
#ifdef SGMAP_DEBUG
		if (__C(SGMAP_TYPE,_debug))
			printf("sgmap_unload:     pte = %p, *pte = 0x%lx\n",
			    pte, (u_long)(*pte));
#endif
		*pte = 0;
	}

	/*
	 * Free the virtual address space used by the mapping
	 * if necessary.
	 */
	if ((map->_dm_flags & BUS_DMA_ALLOCNOW) == 0)
		alpha_sgmap_free(map, sgmap);
	/*
	 * Mark the mapping invalid.
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}
