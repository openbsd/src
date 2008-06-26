/*	$OpenBSD: bus_space.c,v 1.23 2008/06/26 05:42:12 ray Exp $	*/
/*	$NetBSD: bus_space.c,v 1.5 1999/03/26 23:41:30 mycroft Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

int	bus_mem_add_mapping(bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);

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
	paddr_t pa, endpa;
	int error;

	/*
	 * Before we go any further, let's make sure that this
	 * region is available.
	 */
	error = extent_alloc_region(iomem_ex, bpa, size,
	    EX_NOWAIT | (iomem_malloc_safe ? EX_MALLOCOK : 0));
	if (error)
		return (error);

	pa = trunc_page(bpa + t);
	endpa = round_page((bpa + t + size) - 1);

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
	    0, boundary,
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
	vaddr_t va;
	pt_entry_t pte;

	pa = trunc_page(bpa);
	endpa = round_page((bpa + size) - 1);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("bus_mem_add_mapping: overflow");
#endif

	va = uvm_km_valloc(kernel_map, endpa - pa);
	if (va == 0)
		return (ENOMEM);

	bshp->base = (u_long)(va + m68k_page_offset(bpa));
	bshp->swapped = 0;
	bshp->bsr1 = mac68k_bsr1;
	bshp->bsr2 = mac68k_bsr2;
	bshp->bsr4 = mac68k_bsr4;
	bshp->bsrm1 = mac68k_bsrm1;
	bshp->bsrm2 = mac68k_bsrm2;
	bshp->bsrm4 = mac68k_bsrm4;
	bshp->bsrr1 = mac68k_bsrr1;
	bshp->bsrr2 = mac68k_bsrr2;
	bshp->bsrr4 = mac68k_bsrr4;
	bshp->bsw1 = mac68k_bsw1;
	bshp->bsw2 = mac68k_bsw2;
	bshp->bsw4 = mac68k_bsw4;
	bshp->bswm1 = mac68k_bswm1;
	bshp->bswm2 = mac68k_bswm2;
	bshp->bswm4 = mac68k_bswm4;
	bshp->bswr1 = mac68k_bswr1;
	bshp->bswr2 = mac68k_bswr2;
	bshp->bswr4 = mac68k_bswr4;
	bshp->bssm1 = mac68k_bssm1;
	bshp->bssm2 = mac68k_bssm2;
	bshp->bssm4 = mac68k_bssm4;
	bshp->bssr1 = mac68k_bssr1;
	bshp->bssr2 = mac68k_bssr2;
	bshp->bssr4 = mac68k_bssr4;

	if (flags & BUS_SPACE_MAP_CACHEABLE)
		pte = PG_CWT;
	else
		pte = PG_CI;

	for (; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_enter_cache(pmap_kernel(), va, pa,
		    UVM_PROT_RW, UVM_PROT_RW | PMAP_WIRED, pte);
	pmap_update(pmap_kernel());
 
	return 0;
}

void
bus_space_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	vaddr_t va, endva;
	bus_addr_t bpa;

	va = trunc_page(bsh.base);
	endva = round_page((bsh.base + size) - 1);

#ifdef DIAGNOSTIC
	if (endva <= va)
		panic("bus_space_unmap: overflow");
#endif

	pmap_extract(pmap_kernel(), va, &bpa);
	bpa += m68k_page_offset(bsh.base);

	/*
	 * Free the kernel virtual mapping.
	 */
	pmap_remove(pmap_kernel(), va, endva);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, endva - va);

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
	*nbshp = bsh;
	nbshp->base += offset;
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

void
mac68k_bus_space_handle_swapped(t, h)
	bus_space_tag_t t;
	bus_space_handle_t *h;
{
	h->swapped = 1;
	h->bsr2 = mac68k_bsr2_swap;
	h->bsr4 = mac68k_bsr4_swap;
	h->bsrm2 = mac68k_bsrm2_swap;
	h->bsrm4 = mac68k_bsrm4_swap;
	h->bsrr2 = mac68k_bsrr2_swap;
	h->bsrr4 = mac68k_bsrr4_swap;
	h->bsw2 = mac68k_bsw2_swap;
	h->bsw4 = mac68k_bsw4_swap;
	h->bswm2 = mac68k_bswm2_swap;
	h->bswm4 = mac68k_bswm4_swap;
	h->bswr2 = mac68k_bswr2_swap;
	h->bswr4 = mac68k_bswr4_swap;
	h->bssm2 = mac68k_bssm2_swap;
	h->bssm4 = mac68k_bssm4_swap;
	h->bssr2 = mac68k_bssr2_swap;
	h->bssr4 = mac68k_bssr4_swap;
}

u_int8_t
mac68k_bsr1(bus_space_tag_t t, bus_space_handle_t *bsh, bus_size_t offset)
{
	return (*(volatile u_int8_t *) (bsh->base + offset));
}

u_int16_t
mac68k_bsr2(bus_space_tag_t t, bus_space_handle_t *bsh, bus_size_t offset)
{
	return (*(volatile u_int16_t *) (bsh->base + offset));
}

u_int16_t
mac68k_bsr2_swap(bus_space_tag_t t, bus_space_handle_t *bsh, bus_size_t offset)
{
	u_int16_t	v;

	v = (*(volatile u_int16_t *) (bsh->base + offset));
	return swap16(v);
}

u_int32_t
mac68k_bsr4(bus_space_tag_t tag, bus_space_handle_t *bsh, bus_size_t offset)
{
	return (*(volatile u_int32_t *) (bsh->base + offset));
}

u_int32_t
mac68k_bsr4_swap(bus_space_tag_t t, bus_space_handle_t *bsh, bus_size_t offset)
{
	u_int32_t	v;

	v = (*(volatile u_int32_t *) (bsh->base + offset));
	return swap32(v);
}

void
mac68k_bsrm1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movb	a0@,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrm2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrm2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@,d1	;"
	"	rolw	#8,d1		;"
	"	movw	d1,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bsrm4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,%d0		;"
	"1:	movl	a0@,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrm4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a0@,d1	;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	d1,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bsrr1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movb	a0@+,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrr2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@+,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrr2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@+,d1	;"
	"	rolw	#8,d1		;"
	"	movw	d1,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bsrr4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a0@+,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsrr4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a0@+,d1	;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	d1,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bsw1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	    u_int8_t v)
{
	(*(volatile u_int8_t *)(h->base + offset)) = v;
}

void
mac68k_bsw2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	    u_int16_t v)
{
	(*(volatile u_int16_t *)(h->base + offset)) = v;
}

void
mac68k_bsw2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		 u_int16_t v)
{
	v = swap16(v);
	(*(volatile u_int16_t *)(h->base + offset)) = v;
}

void
mac68k_bsw4(bus_space_tag_t tag, bus_space_handle_t *h, bus_size_t offset,
	    u_int32_t v)
{
	(*(volatile u_int32_t *)(h->base + offset)) = v;
}

void
mac68k_bsw4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		 u_int32_t v)
{
	v = swap32(v);
	(*(volatile u_int32_t *)(h->base + offset)) = v;
}

void
mac68k_bswm1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movb	a1@+,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswm2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswm2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  const u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,d1	;"
	"	rolw	#8,d1		;"
	"	movw	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bswm4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswm4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  const u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,d1	;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bswr1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movb	a1@+,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswr2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswr2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  const u_int16_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,d1	;"
	"	rolw	#8,d1		;"
	"	movw	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bswr4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     const u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
mac68k_bswr4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  const u_int32_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,d1	;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h->base + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0","d1");
}

void
mac68k_bssm1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int8_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movb	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssm2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int16_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movw	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssm2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int16_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	rolw	#8,d1		;"
	"	movl	%2,d0		;"
	"1:	movw	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssm4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int32_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movl	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssm4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int32_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	%2,d0		;"
	"1:	movl	d1,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssr1(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int8_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movb	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssr2(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int16_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movw	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssr2_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int16_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	rolw	#8,d1		;"
	"	movl	%2,d0		;"
	"1:	movw	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssr4(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
	     u_int32_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	movl	%2,d0		;"
	"1:	movl	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
mac68k_bssr4_swap(bus_space_tag_t t, bus_space_handle_t *h, bus_size_t offset,
		  u_int32_t v, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,d1		;"
	"	rolw	#8,d1		;"
	"	swap	d1		;"
	"	rolw	#8,d1		;"
	"	movl	%2,d0		;"
	"1:	movl	d1,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"						:
									:
		    "r" (h->base + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}
