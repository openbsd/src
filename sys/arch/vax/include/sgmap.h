/*	$OpenBSD: sgmap.h,v 1.7 2008/06/26 05:42:14 ray Exp $	*/
/* $NetBSD: sgmap.h,v 1.3 2000/05/17 21:22:18 matt Exp $ */

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

#ifndef	_VAX_COMMON_SGMAPVAR_H
#define	_VAX_COMMON_SGMAPVAR_H

#include <sys/extent.h>
#include <machine/bus.h>
#include <machine/pte.h>

/*
 * A VAX SGMAP's state information.  Nothing in the sgmap requires
 * locking[*], with the exception of the extent map.  Locking of the
 * extent map is handled within the extent manager itself.
 *
 * [*] While the page table is a `global' resource, access to it is
 * controlled by the extent map; once a region has been allocated from
 * the map, that region is effectively `locked'.
 */
struct vax_sgmap {
	struct extent *aps_ex;		/* extent map to manage sgva space */
	pt_entry_t *aps_pt;		/* page table */
	bus_addr_t aps_sgvabase;	/* base of the sgva space */
	bus_size_t aps_sgvasize;	/* size of the sgva space */
	bus_addr_t aps_pa;		/* Address in region */
	unsigned int aps_flags;		/* flags */
};

void	vax_sgmap_init(bus_dma_tag_t, struct vax_sgmap *,
	    const char *, bus_addr_t, bus_size_t, pt_entry_t *, bus_size_t);

int	vax_sgmap_alloc(bus_dmamap_t, bus_size_t,
	    struct vax_sgmap *, int);
void	vax_sgmap_free(bus_dmamap_t, struct vax_sgmap *);

int     vax_sgmap_load(bus_dma_tag_t, bus_dmamap_t, void *, 
	    bus_size_t, struct proc *, int, struct vax_sgmap *);

int	vax_sgmap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int, struct vax_sgmap *);
   
int     vax_sgmap_load_uio(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int, struct vax_sgmap *);
         
int	vax_sgmap_load_raw(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int, struct vax_sgmap *);

void	vax_sgmap_unload( bus_dma_tag_t, bus_dmamap_t, 
	    struct vax_sgmap *);

#endif	/* _VAX_COMMON_SGMAPVAR_H */
