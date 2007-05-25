/*	$OpenBSD: footbridge_io.c,v 1.2 2007/05/25 16:22:27 krw Exp $	*/
/*	$NetBSD: footbridge_io.c,v 1.6 2002/04/12 19:12:31 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Causality Limited
 * Copyright (c) 1997 Mark Brinicombe.
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * bus_space I/O functions for footbridge
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <arm/footbridge/footbridge.h>
#include <arm/footbridge/dc21285mem.h>
#include <uvm/uvm_extern.h>

/* Prototypes for all the bus_space structure functions */

bs_protos(footbridge);
bs_protos(generic);
bs_protos(generic_armv4);
bs_protos(bs_notimpl);
bs_map_proto(footbridge_mem);
bs_unmap_proto(footbridge_mem);

/* Declare the footbridge bus space tag */

struct bus_space footbridge_bs_tag = {
	/* cookie */
	(void *) 0,			/* Base address */

	/* mapping/unmapping */
	footbridge_bs_map,
	footbridge_bs_unmap,
	footbridge_bs_subregion,

	/* allocation/deallocation */
	footbridge_bs_alloc,
	footbridge_bs_free,

	/* get kernel virtual address */
	footbridge_bs_vaddr,

	/* Mmap bus space for user */
	bs_notimpl_bs_mmap,
	
	/* barrier */
	footbridge_bs_barrier,

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
	bs_notimpl_bs_rr_1,
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
	bs_notimpl_bs_wr_1,
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
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	generic_armv4_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

void footbridge_create_io_bs_tag(t, cookie)
	struct bus_space *t;
	void *cookie;
{
	*t = footbridge_bs_tag;
	t->bs_cookie = cookie;
}

void footbridge_create_mem_bs_tag(t, cookie)
	struct bus_space *t;
	void *cookie;
{
	*t = footbridge_bs_tag;
	t->bs_map = footbridge_mem_bs_map;
	t->bs_unmap = footbridge_mem_bs_unmap;
	t->bs_cookie = cookie;
}

/* bus space functions */

int
footbridge_bs_map(t, bpa, size, cacheable, bshp)
	void *t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	/*
	 * The whole 64K of PCI space is always completely mapped during
	 * boot.
	 *
	 * Eventually this function will do the mapping check overlapping / 
	 * multiple mappings.
	 */

	/* The cookie is the base address for the I/O area */
	*bshp = bpa + (bus_addr_t)t;
	return(0);
}

int
footbridge_mem_bs_map(t, bpa, size, cacheable, bshp)
	void *t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	bus_addr_t startpa, endpa;
	vaddr_t va;

	/* Round the allocation to page boundaries */
	startpa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	/*
	 * Check for mappings below 1MB as we have this space already
	 * mapped. In practice it is only the VGA hole that takes
	 * advantage of this.
	 */
	if (endpa < DC21285_PCI_ISA_MEM_VSIZE) {
		/* Store the bus space handle */
		*bshp = DC21285_PCI_ISA_MEM_VBASE + bpa;
		return 0;
	}

	/*
	 * Eventually this function will do the mapping check for overlapping / 
	 * multiple mappings
	 */

	va = uvm_km_valloc(kernel_map, endpa - startpa);
	if (va == 0)
		return ENOMEM;

	/* Store the bus space handle */
	*bshp = va + (bpa & PGOFSET);

	/*
	 * Now map the pages. The cookie is the physical base address for the
	 * I/O area.
	 */
	while (startpa < endpa) {
		pmap_enter(pmap_kernel(), va, (bus_addr_t)t + startpa,
		    VM_PROT_READ | VM_PROT_WRITE, 0);
		va += PAGE_SIZE;
		startpa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

/*	if (bpa >= DC21285_PCI_MEM_VSIZE && bpa != DC21285_ARMCSR_VBASE)
		panic("footbridge_bs_map: Address out of range (%08lx)", bpa);
*/
	return(0);
}

int
footbridge_bs_alloc(t, rstart, rend, size, alignment, boundary, cacheable,
    bpap, bshp)
	void *t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int cacheable;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	panic("footbridge_alloc(): Help!");
}


void
footbridge_bs_unmap(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/*
	 * Temporary implementation
	 */
}

void
footbridge_mem_bs_unmap(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	vaddr_t startva, endva;

	/*
	 * Check for mappings below 1MB as we have this space permenantly
	 * mapped. In practice it is only the VGA hole that takes
	 * advantage of this.
	 */
	if (bsh >= DC21285_PCI_ISA_MEM_VBASE
	    && bsh < (DC21285_PCI_ISA_MEM_VBASE + DC21285_PCI_ISA_MEM_VSIZE)) {
		return;
	}

	startva = trunc_page(bsh);
	endva = round_page(bsh + size);

	uvm_km_free(kernel_map, startva, endva - startva);
}

void    
footbridge_bs_free(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	panic("footbridge_free(): Help!");
	/* footbridge_bs_unmap() does all that we need to do. */
/*	footbridge_bs_unmap(t, bsh, size);*/
}

int
footbridge_bs_subregion(t, bsh, offset, size, nbshp)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + (offset << ((int)t));
	return (0);
}

void *
footbridge_bs_vaddr(t, bsh)
	void *t;
	bus_space_handle_t bsh;
{

	return ((void *)bsh);
}

void
footbridge_bs_barrier(t, bsh, offset, len, flags)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, len;
	int flags;
{
}	
