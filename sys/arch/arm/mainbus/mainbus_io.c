/*	$OpenBSD: mainbus_io.c,v 1.3 2007/05/25 16:45:59 krw Exp $ */
/*	$NetBSD: mainbus_io.c,v 1.14 2003/12/06 22:05:33 bjh21 Exp $	*/

/*
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
 *	This product includes software developed by Mark Brinicombe.
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
 * bus_space I/O functions for mainbus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>

#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/pmap.h>

/* Prototypes for all the bus_space structure functions */

bs_protos(mainbus);
bs_protos(bs_notimpl);

/* Declare the mainbus bus space tag */

struct bus_space mainbus_bs_tag = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	mainbus_bs_map,
	mainbus_bs_unmap,
	mainbus_bs_subregion,

	/* allocation/deallocation */
	mainbus_bs_alloc,
	mainbus_bs_free,

	/* get kernel virtual address */
	0, /* there is no linear mapping */

	/* Mmap bus space for user */
	mainbus_bs_mmap,

	/* barrier */
	mainbus_bs_barrier,

	/* read (single) */
	mainbus_bs_r_1,
	mainbus_bs_r_2,
	mainbus_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	bs_notimpl_bs_rm_1,
	mainbus_bs_rm_2,
	bs_notimpl_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	bs_notimpl_bs_rr_1,
	bs_notimpl_bs_rr_2,
	bs_notimpl_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	mainbus_bs_w_1,
	mainbus_bs_w_2,
	mainbus_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	mainbus_bs_wm_1,
	mainbus_bs_wm_2,
	bs_notimpl_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	bs_notimpl_bs_wr_1,
	bs_notimpl_bs_wr_2,
	bs_notimpl_bs_wr_4,
	bs_notimpl_bs_wr_8,

	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	bs_notimpl_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	bs_notimpl_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

/* bus space functions */

int
mainbus_bs_map(t, bpa, size, flags, bshp)
	void *t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	u_long startpa, endpa, pa;
	vaddr_t va;
	pt_entry_t *pte;

	if ((u_long)bpa > (u_long)KERNEL_BASE) {
		/* XXX This is a temporary hack to aid transition. */
		*bshp = bpa;
		return(0);
	}

	startpa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	/* XXX use extent manager to check duplicate mapping */

	va = uvm_km_valloc(kernel_map, endpa - startpa);
	if (! va)
		return(ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	for(pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE);
		if ((flags & BUS_SPACE_MAP_CACHEABLE) == 0) {
			pte = vtopte(va);
			*pte &= ~L2_S_CACHE_MASK;
			PTE_SYNC(pte);
		}
	}
	pmap_update(pmap_kernel());

	return(0);
}

int
mainbus_bs_alloc(t, rstart, rend, size, alignment, boundary, cacheable,
    bpap, bshp)
	void *t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int cacheable;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	panic("mainbus_bs_alloc(): Help!");
}


void
mainbus_bs_unmap(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/*
	 * Temporary implementation
	 */
}

void    
mainbus_bs_free(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	panic("mainbus_bs_free(): Help!");
	/* mainbus_bs_unmap() does all that we need to do. */
/*	mainbus_bs_unmap(t, bsh, size);*/
}

int
mainbus_bs_subregion(t, bsh, offset, size, nbshp)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + (offset << 2);
	return (0);
}

paddr_t
mainbus_bs_mmap(t, paddr, offset, prot, flags)
	void *t;
	bus_addr_t paddr;
	off_t offset;
	int prot;
	int flags;
{
	/*
	 * mmap from address `paddr+offset' for one page
	 */
	 return (atop(paddr + offset));
}

void
mainbus_bs_barrier(t, bsh, offset, len, flags)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, len;
	int flags;
{
}	

/* End of mainbus_io.c */
