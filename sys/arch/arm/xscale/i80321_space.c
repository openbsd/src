/*	$OpenBSD: i80321_space.c,v 1.5 2014/11/16 12:30:56 deraadt Exp $	*/
/*	$NetBSD: i80321_space.c,v 1.9 2005/11/24 13:08:32 yamt Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * bus_space functions for i80321 I/O Processor.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(i80321);
bs_protos(i80321_io);
bs_protos(i80321_mem);
bs_protos(generic);
bs_protos(generic_armv4);
bs_protos(bs_notimpl);

/*
 * Template bus_space -- copied, and the bits that are NULL are
 * filled in.
 */
const struct bus_space i80321_bs_tag_template = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	i80321_bs_map,
	i80321_bs_unmap,
	i80321_bs_subregion,

	/* allocation/deallocation */
	NULL,
	NULL,

	/* get kernel virtual address */
	i80321_bs_vaddr,

	/* mmap */
	i80321_bs_mmap,

	/* barrier */
	i80321_bs_barrier,

	/* read (single) */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	generic_bs_wr_1,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	generic_armv4_bs_sr_2,
	generic_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	generic_armv4_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

void
i80321_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80321_bs_tag_template;
	bs->bs_cookie = cookie;
}

void
i80321_io_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80321_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i80321_io_bs_map;
	bs->bs_unmap = i80321_io_bs_unmap;
	bs->bs_alloc = i80321_io_bs_alloc;
	bs->bs_free = i80321_io_bs_free;

	bs->bs_vaddr = i80321_io_bs_vaddr;
}

void
i80321_mem_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80321_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i80321_mem_bs_map;
	bs->bs_unmap = i80321_mem_bs_unmap;
	bs->bs_alloc = i80321_mem_bs_alloc;
	bs->bs_free = i80321_mem_bs_free;

	bs->bs_mmap = i80321_mem_bs_mmap;
}

int
i80321_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flag,
    bus_space_handle_t *bshp)
{
	const struct pmap_devmap *pd;
	paddr_t startpa, endpa, pa, pagecnt;
	vaddr_t va;
	pt_entry_t *pte;

	if ((pd = pmap_devmap_find_pa(bpa, size)) != NULL) {
		/* Device was statically mapped. */
		*bshp = pd->pd_va + (bpa - pd->pd_pa);
		return (0);
	}

#if 0
printf("i80321_bs_map bpa %x, size %x flag %x\n", bpa, size, flag);
#endif
	endpa = round_page(bpa + size);
	startpa = trunc_page(bpa);
	pagecnt = endpa - startpa;

	va = uvm_km_valloc(kernel_map, endpa - startpa);
	if (va == 0)
		return(ENOMEM);
#if 0
printf("i80321_bs_map va %x pa %x, endpa %x, sz %x\n", va, startpa,
    endpa, endpa-startpa);
#endif

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	for (pa = startpa; pagecnt > 0;
	    pa += PAGE_SIZE, va += PAGE_SIZE, pagecnt -= PAGE_SIZE) {
		pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
		if ((flag & BUS_SPACE_MAP_CACHEABLE) == 0) {
			pte = vtopte(va);
			*pte &= ~L2_S_CACHE_MASK;
			PTE_SYNC(pte);
		}
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
i80321_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t va, endva;

	if (pmap_devmap_find_va(bsh, size) != NULL) {
		/* Device was statically mapped; nothing to do. */
		return;
	}

	endva = round_page(bsh + size);
	va = trunc_page(bsh);

	pmap_kremove(va, endva - va);
	uvm_km_free(kernel_map, va, endva - va);
}


/* *** Routines shared by i80321, PCI IO, and PCI MEM. *** */

int
i80321_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void
i80321_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}

void *
i80321_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	return ((void *)bsh);
}

paddr_t
i80321_bs_mmap(void *t, bus_addr_t addr, off_t off, int prot, int flags)
{

	/* Not supported. */
	return (-1);
}

/* *** Routines for PCI IO. *** */

int
i80321_io_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	struct i80321_softc *sc = t;
	uint32_t busbase;

	if (bpa >= sc->sc_ioout_xlate &&
	    bpa < (sc->sc_ioout_xlate + VERDE_OUT_XLATE_IO_WIN_SIZE)) {
		busbase = sc->sc_ioout_xlate;
	} else 
		return (EINVAL);

	if ((bpa + size) >= (busbase + VERDE_OUT_XLATE_IO_WIN_SIZE))
		return (EINVAL);

	/*
	 * Found the window -- PCI I/O space is mapped at a fixed
	 * virtual address by board-specific code.  Translate the
	 * bus address to the virtual address.
	 */
	bus_space_subregion(sc->sc_st, sc->sc_io_sh, (bpa - busbase), size,
	    bshp);
	return (0);
}

void
i80321_io_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	/* Nothing to do. */
}

int
i80321_io_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80321_io_bs_alloc(): not implemented");
}

void    
i80321_io_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80321_io_bs_free(): not implemented");
}

void *
i80321_io_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	/* Not supported. */
	return (NULL);
}

/* *** Routines for PCI MEM. *** */

int
i80321_mem_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flag,
    bus_space_handle_t *bshp)
{

	struct i80321_softc *sc = t;
	vaddr_t va;
	uint32_t busbase;
	paddr_t pa, endpa, physbase;
	pt_entry_t *pte;

#if 0
printf("i80321_bs_map bpa %x, size %x flag %x : %x %x \n", bpa, size, flag,
   sc->sc_owin[0].owin_xlate_lo,
   sc->sc_owin[0].owin_xlate_lo+ VERDE_OUT_XLATE_MEM_WIN_SIZE);
#endif

	if (bpa >= sc->sc_owin[0].owin_xlate_lo &&
	    bpa < (sc->sc_owin[0].owin_xlate_lo +
		   VERDE_OUT_XLATE_MEM_WIN_SIZE)) {
		busbase = sc->sc_iwin[1].iwin_xlate;
		physbase = sc->sc_owin[0].owin_xlate_lo;
	} else
		return (EINVAL);

	if ((bpa + size) >= ( sc->sc_owin[0].owin_xlate_lo +
	    VERDE_OUT_XLATE_MEM_WIN_SIZE))
		return (EINVAL);

	/*
	 * Found the window -- PCI MEM space is now mapped by allocating
	 * some kernel VA space and mapping the pages with pmap_enter().
	 * pmap_enter() will map unmanaged pages as non-cacheable.
	 */
	pa = trunc_page((bpa - busbase) + physbase);
	endpa = round_page(((bpa - busbase) + physbase) + size);

	va = uvm_km_valloc(kernel_map, endpa - pa);
	if (va == 0)
		return (ENOMEM);
//printf("i80321_mem_bs_map bpa %x pa %x va %x sz %x\n", bpa, pa, va, endpa-pa);

#if 0
printf("i80321_bs_map va %x pa %x, endpa %x, sz %x\n", va, pa,
    endpa, endpa-pa);
#endif

	*bshp = va + (bpa & PAGE_MASK);

	for (; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
		if ((flag & BUS_SPACE_MAP_CACHEABLE) == 0) {
			pte = vtopte(va);
			*pte &= ~L2_S_CACHE_MASK;
			PTE_SYNC(pte);
		}
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
i80321_mem_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t va, endva;

	va = trunc_page(bsh);
	endva = round_page(bsh + size);

	pmap_kremove(va, endva - va);
	pmap_update(pmap_kernel());

	/* Free the kernel virtual mapping. */
	uvm_km_free(kernel_map, va, endva - va);
}

int
i80321_mem_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80321_mem_bs_alloc(): not implemented");
}

void    
i80321_mem_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80321_mem_bs_free(): not implemented");
}

paddr_t
i80321_mem_bs_mmap(void *t, bus_addr_t addr, off_t off, int prot, int flags)
{

	/* XXX */
	return (-1);
}
