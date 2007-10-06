/* $OpenBSD: tc_dma_3000_500.c,v 1.3 2007/10/06 23:50:54 krw Exp $ */
/* $NetBSD: tc_dma_3000_500.c,v 1.13 2001/07/19 06:40:03 thorpej Exp $ */

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

#define _ALPHA_BUS_DMA_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/tc/tcvar.h>
#include <alpha/tc/tc_sgmap.h>
#include <alpha/tc/tc_dma_3000_500.h>

struct alpha_bus_dma_tag tc_dmat_sgmap = {
	NULL,				/* _cookie */
	0,				/* _wbase */
	0,				/* _wsize */
	NULL,				/* _next_window */
	0,				/* _boundary */
	NULL,				/* _sgmap */
	0,				/* _pfthresh */
	NULL,				/* _get_tag */
	tc_bus_dmamap_create_sgmap,
	tc_bus_dmamap_destroy_sgmap,
	tc_bus_dmamap_load_sgmap,
	tc_bus_dmamap_load_mbuf_sgmap,
	tc_bus_dmamap_load_uio_sgmap,
	tc_bus_dmamap_load_raw_sgmap,
	tc_bus_dmamap_unload_sgmap,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

struct tc_dma_slot_info {
	struct alpha_sgmap tdsi_sgmap;		/* sgmap for slot */
	struct alpha_bus_dma_tag tdsi_dmat;	/* dma tag for slot */
};
struct tc_dma_slot_info *tc_dma_slot_info;

void
tc_dma_init_3000_500(nslots)
	int nslots;
{
	extern struct alpha_bus_dma_tag tc_dmat_direct;
	size_t sisize;
	int i;

	/* Allocate per-slot DMA info. */
	sisize = nslots * sizeof(struct tc_dma_slot_info);
	tc_dma_slot_info = malloc(sisize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (tc_dma_slot_info == NULL)
		panic("tc_dma_init: can't allocate per-slot DMA info");

	/* Default all slots to direct-mapped. */
	for (i = 0; i < nslots; i++)
		memcpy(&tc_dma_slot_info[i].tdsi_dmat, &tc_dmat_direct,
		    sizeof(tc_dma_slot_info[i].tdsi_dmat));
}

/*
 * Return the DMA tag for the given slot.
 */
bus_dma_tag_t
tc_dma_get_tag_3000_500(slot)
	int slot;
{

	return (&tc_dma_slot_info[slot].tdsi_dmat);
}

/*
 * Create a TurboChannel SGMAP-mapped DMA map.
 */
int
tc_bus_dmamap_create_sgmap(t, size, nsegments, maxsegsz, boundary,
    flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	bus_dmamap_t map;
	int error;

	error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
	    boundary, flags, dmamp);
	if (error)
		return (error);

	map = *dmamp;

	/* XXX BUS_DMA_ALLOCNOW */

	return (error);
}

/*
 * Destroy a TurboChannel SGMAP-mapped DMA map.
 */
void
tc_bus_dmamap_destroy_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	KASSERT(map->dm_mapsize == 0);

	_bus_dmamap_destroy(t, map);
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with a linear buffer.
 */
int
tc_bus_dmamap_load_sgmap(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load(t, map, buf, buflen, p, flags,
	    &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with an mbuf chain.
 */
int
tc_bus_dmamap_load_mbuf_sgmap(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_mbuf(t, map, m, flags, &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with a uio.
 */
int
tc_bus_dmamap_load_uio_sgmap(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_uio(t, map, uio, flags, &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with raw memory.
 */
int
tc_bus_dmamap_load_raw_sgmap(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_raw(t, map, segs, nsegs, size, flags,
	    &tdsi->tdsi_sgmap));
}

/*
 * Unload a TurboChannel SGMAP-mapped DMA map.
 */
void
tc_bus_dmamap_unload_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	/*
	 * Invalidate any SGMAP page table entries used by this
	 * mapping.
	 */
	tc_sgmap_unload(t, map, &tdsi->tdsi_sgmap);

	/*
	 * Do the generic bits of the unload.
	 */
	_bus_dmamap_unload(t, map);
}
