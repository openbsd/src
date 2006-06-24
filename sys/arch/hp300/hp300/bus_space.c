/*	$OpenBSD: bus_space.c,v 1.4 2006/06/24 13:20:17 miod Exp $	*/
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
extern int *nofault;

/*
 * Memory mapped devices (intio, dio and sgc)
 */
int
bus_space_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	u_long kva;
	pt_entry_t template;
	int error;

	switch (HP300_TAG_BUS(t)) {
	case HP300_BUS_INTIO:
		/*
		 * intio space is direct-mapped in pmap_bootstrap(); just
		 * do the translation in this case.
		 */
		*bshp = IIOV(INTIOBASE + bpa);
		return (0);
	default:
		break;
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
bus_space_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
#ifdef DIAGNOSTIC
	extern int eiomapsize;
#endif
	int error;

	switch (HP300_TAG_BUS(t)) {
	case HP300_BUS_INTIO:
		/*
		 * intio space is direct-mapped in pmap_bootstrap(); nothing
		 * to do.
		 */
		return;
	default:
		break;
	}

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
bus_space_subregion(t, bsh, offset, size, nbshp)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}
