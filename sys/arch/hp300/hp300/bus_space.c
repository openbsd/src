/*	$OpenBSD: bus_space.c,v 1.7 2010/12/26 15:40:59 miod Exp $	*/
/*	$NetBSD: bus_space.c,v 1.6 2002/09/27 15:36:02 provos Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Implementation of bus_space mapping for the hp300.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#ifdef DIAGNOSTIC
extern char *extiobase;
#endif
extern struct extent *extio;

/*
 * Memory mapped devices (intio, dio and sgc)
 */

int	hp300_mem_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	hp300_mem_unmap(bus_space_handle_t, bus_size_t);
int	hp300_mem_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void *	hp300_mem_vaddr(bus_space_handle_t);

u_int8_t hp300_mem_bsr1(bus_space_handle_t, bus_size_t);
u_int16_t hp300_mem_bsr2(bus_space_handle_t, bus_size_t);
u_int32_t hp300_mem_bsr4(bus_space_handle_t, bus_size_t);
void	hp300_mem_bsrm1(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsrm2(bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
void	hp300_mem_bsrm4(bus_space_handle_t, bus_size_t, u_int32_t *, size_t);
void	hp300_mem_bsrrm2(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsrrm4(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsrr1(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsrr2(bus_space_handle_t, bus_size_t, u_int16_t *, size_t);
void	hp300_mem_bsrr4(bus_space_handle_t, bus_size_t, u_int32_t *, size_t);
void	hp300_mem_bsrrr2(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsrrr4(bus_space_handle_t, bus_size_t, u_int8_t *, size_t);
void	hp300_mem_bsw1(bus_space_handle_t, bus_size_t, u_int8_t);
void	hp300_mem_bsw2(bus_space_handle_t, bus_size_t, u_int16_t);
void	hp300_mem_bsw4(bus_space_handle_t, bus_size_t, u_int32_t);
void	hp300_mem_bswm1(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bswm2(bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
void	hp300_mem_bswm4(bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);
void	hp300_mem_bswrm2(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bswrm4(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bswr1(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bswr2(bus_space_handle_t, bus_size_t, const u_int16_t *, size_t);
void	hp300_mem_bswr4(bus_space_handle_t, bus_size_t, const u_int32_t *, size_t);
void	hp300_mem_bswrr2(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bswrr4(bus_space_handle_t, bus_size_t, const u_int8_t *, size_t);
void	hp300_mem_bssm1(bus_space_handle_t, bus_size_t, u_int8_t, size_t);
void	hp300_mem_bssm2(bus_space_handle_t, bus_size_t, u_int16_t, size_t);
void	hp300_mem_bssm4(bus_space_handle_t, bus_size_t, u_int32_t, size_t);
void	hp300_mem_bssr1(bus_space_handle_t, bus_size_t, u_int8_t, size_t);
void	hp300_mem_bssr2(bus_space_handle_t, bus_size_t, u_int16_t, size_t);
void	hp300_mem_bssr4(bus_space_handle_t, bus_size_t, u_int32_t, size_t);

struct hp300_bus_space_tag hp300_mem_tag = {
	hp300_mem_map,
	hp300_mem_unmap,
	hp300_mem_subregion,
	hp300_mem_vaddr,

	hp300_mem_bsr1,
	hp300_mem_bsr2,
	hp300_mem_bsr4,
	hp300_mem_bsrm1,
	hp300_mem_bsrm2,
	hp300_mem_bsrm4,
	hp300_mem_bsrrm2,
	hp300_mem_bsrrm4,
	hp300_mem_bsrr1,
	hp300_mem_bsrr2,
	hp300_mem_bsrr4,
	hp300_mem_bsrrr2,
	hp300_mem_bsrrr4,
	hp300_mem_bsw1,
	hp300_mem_bsw2,
	hp300_mem_bsw4,
	hp300_mem_bswm1,
	hp300_mem_bswm2,
	hp300_mem_bswm4,
	hp300_mem_bswrm2,
	hp300_mem_bswrm4,
	hp300_mem_bswr1,
	hp300_mem_bswr2,
	hp300_mem_bswr4,
	hp300_mem_bswrr2,
	hp300_mem_bswrr4,
	hp300_mem_bssm1,
	hp300_mem_bssm2,
	hp300_mem_bssm4,
	hp300_mem_bssr1,
	hp300_mem_bssr2,
	hp300_mem_bssr4
};

int
hp300_mem_map(bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	u_long kva;
	pt_entry_t template;
	int error;

	/*
	 * intio space is direct-mapped in pmap_bootstrap(); just do the
	 * translation in this case.
	 */
	if (bpa >= INTIOBASE && bpa < INTIOTOP) {
		*bshp = IIOV(bpa);
		return (0);
	}

	/*
	 * Allocate virtual address space from the extio extent map.
	 */
	size = round_page(bpa + size) - trunc_page(bpa);
	error = extent_alloc(extio, size, PAGE_SIZE, 0, EX_NOBOUNDARY,
	    EX_NOWAIT | EX_MALLOCOK, &kva);
	if (error)
		return (error);

	*bshp = (bus_space_handle_t)kva + (bpa & PAGE_MASK);
	bpa = trunc_page(bpa);

	/*
	 * Map the range.
	 */
	if (flags & BUS_SPACE_MAP_CACHEABLE)
		template = PG_RW;
	else
		template = PG_RW | PG_CI;
	while (size != 0) {
		pmap_kenter_cache(kva, bpa, template);
		size -= PAGE_SIZE;
		kva += PAGE_SIZE;
		bpa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	/*
	 * All done.
	 */
	return (0);
}

void
hp300_mem_unmap(bus_space_handle_t bsh, bus_size_t size)
{
#ifdef DIAGNOSTIC
	extern int eiomapsize;
#endif
	int error;

	/*
	 * intio space is direct-mapped in pmap_bootstrap(); nothing
	 * to do.
	 */
	if (IIOP(bsh) >= INTIOBASE && IIOP(bsh) < INTIOTOP)
		return;

#ifdef DIAGNOSTIC
	if ((caddr_t)bsh < extiobase ||
	    (caddr_t)bsh >= extiobase + ptoa(eiomapsize)) {
		printf("bus_space_unmap: bad bus space handle %x\n", bsh);
		return;
	}
#endif

	size = round_page(bsh + size) - trunc_page(bsh);
	bsh = trunc_page(bsh);

	/*
	 * Unmap the range.
	 */
	pmap_kremove(bsh, size);
	pmap_update(pmap_kernel());

	/*
	 * Free it from the extio extent map.
	 */
	error = extent_free(extio, (u_long)bsh, size, EX_NOWAIT | EX_MALLOCOK);
#ifdef DIAGNOSTIC
	if (error != 0) {
		printf("bus_space_unmap: kva 0x%lx size 0x%lx: "
		    "can't free region (%d)\n", (vaddr_t)bsh, size, error);
	}
#endif
}

/* ARGSUSED */
int
hp300_mem_subregion(bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
    bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

#if 0
/* ARGSUSED */
paddr_t
hp300_mem_mmap(bus_addr_t addr, off_t offset, int prot, int flags)
{
	return (((paddr_t)addr + offset) >> PAGE_SHIFT);
}
#endif

void *
hp300_mem_vaddr(bus_space_handle_t h)
{
	return ((void *)h);
}

u_int8_t
hp300_mem_bsr1(bus_space_handle_t bsh, bus_size_t offset)
{
	return (*(volatile u_int8_t *) (bsh + offset));
}

u_int16_t
hp300_mem_bsr2(bus_space_handle_t bsh, bus_size_t offset)
{
	return (*(volatile u_int16_t *) (bsh + offset));
}

u_int32_t
hp300_mem_bsr4(bus_space_handle_t bsh, bus_size_t offset)
{
	return (*(volatile u_int32_t *) (bsh + offset));
}

void
hp300_mem_bsrm1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrm2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrm4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrrm2(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrrm4(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,%d0		;"
	"1:	movl	a0@,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrr1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrr2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrr4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrrr2(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a0@+,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c / 2)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsrrr4(bus_space_handle_t h, bus_size_t offset,
	     u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a0@+,a1@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c / 4)	:
		    "a0","a1","d0");
}

void
hp300_mem_bsw1(bus_space_handle_t h, bus_size_t offset,
	    u_int8_t v)
{
	(*(volatile u_int8_t *)(h + offset)) = v;
}

void
hp300_mem_bsw2(bus_space_handle_t h, bus_size_t offset,
	    u_int16_t v)
{
	(*(volatile u_int16_t *)(h + offset)) = v;
}

void
hp300_mem_bsw4(bus_space_handle_t h, bus_size_t offset,
	    u_int32_t v)
{
	(*(volatile u_int32_t *)(h + offset)) = v;
}

void
hp300_mem_bswm1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswm2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswm4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswrm2(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswrm4(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,a0@	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswr1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswr2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswr4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" (a), "g" (c)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswrr2(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movw	a1@+,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c / 2)	:
		    "a0","a1","d0");
}

void
hp300_mem_bswrr4(bus_space_handle_t h, bus_size_t offset,
	     const u_int8_t *a, size_t c)
{
	__asm __volatile (
	"	movl	%0,a0		;"
	"	movl	%1,a1		;"
	"	movl	%2,d0		;"
	"1:	movl	a1@+,a0@+	;"
	"	subql	#1,d0		;"
	"	jne	1b"					:
								:
		    "r" (h + offset), "g" (a), "g" (c / 4)	:
		    "a0","a1","d0");
}

void
hp300_mem_bssm1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
hp300_mem_bssm2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
hp300_mem_bssm4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
hp300_mem_bssr1(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
hp300_mem_bssr2(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

void
hp300_mem_bssr4(bus_space_handle_t h, bus_size_t offset,
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
		    "r" (h + offset), "g" ((u_long)v), "g" (c)	:
		    "a0","d0","d1");
}

#if 0
paddr_t
bus_space_mmap(bus_space_tag_t t, bus_addr_t addr, off_t offset, int prot,
    int flags)
{
	return ((paddr_t)addr + offset);
}
#endif
