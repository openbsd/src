/*	$OpenBSD: bus_space.c,v 1.2 1999/01/11 05:11:36 millert Exp $	*/
/*	$NetBSD: bus_space.c,v 1.2 1998/04/24 05:27:24 scottr Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
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

/*
 * Implementation of bus_space mapping for mac68k.
 */

#if 0
#include "opt_uvm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>
#include <sys/map.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#endif

int	bus_mem_add_mapping __P((bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *));

extern struct extent *iomem_ex;
extern int iomem_malloc_safe;
label_t *nofault;

int
bus_space_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	u_long pa, endpa;
	int error;

	/*
	 * Before we go any further, let's make sure that this
	 * region is available.
	 */
	error = extent_alloc_region(iomem_ex, bpa, size,
	    EX_NOWAIT | (iomem_malloc_safe ? EX_MALLOCOK : 0));
	if (error)
		return (error);

	pa = mac68k_trunc_page(bpa + t);
	endpa = mac68k_round_page((bpa + t + size) - 1);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("bus_space_map: overflow");
#endif

	error = bus_mem_add_mapping(bpa, size, flags, bshp);
	if (error) {
		if (extent_free(iomem_ex, bpa, size, EX_NOWAIT |
		    (iomem_malloc_safe ? EX_MALLOCOK : 0))) {
			printf("bus_space_map: pa 0x%lx, size 0x%lx\n",
			    bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}

	return (error);
}

int
bus_space_alloc(t, rstart, rend, size, alignment, boundary, flags, bpap, bshp)
	bus_space_tag_t t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int flags;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	u_long bpa;
	int error;

	/*
	 * Sanity check the allocation against the extent's boundaries.
	 */
	if (rstart < iomem_ex->ex_start || rend > iomem_ex->ex_end)
		panic("bus_space_alloc: bad region start/end");

	/*
	 * Do the requested allocation.
	 */
	error = extent_alloc_subregion(iomem_ex, rstart, rend, size, alignment,
	    boundary,
	    EX_FAST | EX_NOWAIT | (iomem_malloc_safe ?  EX_MALLOCOK : 0),
	    &bpa);

	if (error)
		return (error);

	/*
	 * For memory space, map the bus physical address to
	 * a kernel virtual address.
	 */
	error = bus_mem_add_mapping(bpa, size, flags, bshp);
	if (error) {
		if (extent_free(iomem_ex, bpa, size, EX_NOWAIT |
		    (iomem_malloc_safe ? EX_MALLOCOK : 0))) {
			printf("bus_space_alloc: pa 0x%lx, size 0x%lx\n",
			    bpa, size);
			printf("bus_space_alloc: can't free region\n");
		}
	}

	*bpap = bpa;

	return (error);
}

int
bus_mem_add_mapping(bpa, size, flags, bshp)
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	u_long pa, endpa;
	vm_offset_t va;

	pa = mac68k_trunc_page(bpa);
	endpa = mac68k_round_page((bpa + size) - 1);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("bus_mem_add_mapping: overflow");
#endif

#if defined(UVM)
	va = uvm_km_valloc(kernel_map, endpa - pa);
#else
	va = kmem_alloc_pageable(kernel_map, endpa - pa);
#endif
	if (va == 0)
		return (ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa & PGOFSET));

	for (; pa < endpa; pa += NBPG, va += NBPG) {
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, TRUE);
		if (!(flags & BUS_SPACE_MAP_CACHEABLE))
			pmap_changebit(pa, PG_CI, TRUE);
	}
 
	return 0;
}

void
bus_space_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	vm_offset_t	va, endva;
	bus_addr_t bpa;

	va = mac68k_trunc_page(bsh);
	endva = mac68k_round_page((bsh + size) - 1);

#ifdef DIAGNOSTIC
	if (endva <= va)
		panic("bus_space_unmap: overflow");
#endif

	bpa = pmap_extract(pmap_kernel(), va) + (bsh & PGOFSET);

	/*
	 * Free the kernel virtual mapping.
	 */
#if defined(UVM)
	uvm_km_free(kernel_map, va, endva - va);
#else
	kmem_free(kernel_map, va, endva - va);
#endif

	if (extent_free(iomem_ex, bpa, size,
	    EX_NOWAIT | (iomem_malloc_safe ? EX_MALLOCOK : 0))) {
		printf("bus_space_unmap: pa 0x%lx, size 0x%lx\n",
		    bpa, size);
		printf("bus_space_unmap: can't free region\n");
	}
}

void    
bus_space_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/* bus_space_unmap() does all that we need to do. */
	bus_space_unmap(t, bsh, size);
}

int
bus_space_subregion(t, bsh, offset, size, nbshp)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}

int
mac68k_bus_space_probe(t, bsh, offset, sz)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t offset;
	int sz;
{
	int i;
	label_t faultbuf;

	nofault = &faultbuf;
	if (setjmp(nofault)) {
		nofault = (label_t *)0;
		return (0);
	}

	switch (sz) {
	case 1:
		i = bus_space_read_1(t, bsh, offset);
		break;
	case 2:
		i = bus_space_read_2(t, bsh, offset);
		break;
	case 4:
		i = bus_space_read_4(t, bsh, offset);
		break;
	case 8:
	default:
		panic("bus_space_probe: unsupported data size %d", sz);
		/* NOTREACHED */
	}

	nofault = (label_t *)0;
	return (1);
}
