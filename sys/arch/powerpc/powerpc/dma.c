/*	$OpenBSD: dma.c,v 1.1 2000/03/20 07:05:52 rahnds Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/extent.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#ifdef UVM
#include <uvm/uvm.h>
#include <uvm/uvm_page.h>
#else
#endif

#include <machine/bus.h>

int
_dmamap_create(v, size, nsegments, maxsegsz, boundary, flags, dmamp)
	void *v;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	register struct powerpc_bus_dmamap *map;
	register size_t mapsize;

	mapsize = sizeof(struct powerpc_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	MALLOC(map, struct powerpc_bus_dmamap *, mapsize, M_DEVBUF,
		(flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	if (!map)
		return (ENOMEM);

	bzero(map, mapsize);
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);

	*dmamp = map;
	return (0);
}

void
_dmamap_destroy(v, map)
	void *v;
	bus_dmamap_t map;
{
	free(map, M_DEVBUF);
}

int
_dmamap_load(v, map, addr, size, p, flags)
	void *v;
	bus_dmamap_t map;
	void *addr;
	bus_size_t size;
	struct proc *p;
	int flags;
{
	panic("_dmamap_load: not implemented");
}

int
_dmamap_load_mbuf(v, map, m, flags)
	void *v;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	panic("_dmamap_load_mbuf: not implemented");
}

int
_dmamap_load_uio(v, map, uio, flags)
	void *v;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	panic("_dmamap_load_uio: not implemented");
}

int
_dmamap_load_raw(v, map, segs, nsegs, size, flags)
	void *v;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	panic("_dmamap_load_raw: not implemented");
}

void
_dmamap_unload(v, map)
	void *v;
	bus_dmamap_t map;
{
	panic("_dmamap_unload: not implemented");
}

void
_dmamap_sync(v, map, ops)
	void *v;
	bus_dmamap_t map;
	bus_dmasync_op_t ops;
{
#if 0
	__asm __volatile ("syncdma");
#endif
}

int
_dmamem_alloc(v, size, alignment, boundary, segs, nsegs, rsegs, flags)
	void *v;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vaddr_t va;
	paddr_t spa, epa;

	size = round_page(size);

#if defined(UVM)
	va = uvm_pagealloc_contig(size, VM_MIN_KERNEL_ADDRESS,
					VM_MAX_KERNEL_ADDRESS, NBPG);
#else
	vm_page_alloc_memory(size, VM_MIN_KERNEL_ADDRESS,
		VM_MAX_KERNEL_ADDRESS,
	    alignment, boundary, (void *)&va, nsegs, (flags & BUS_DMA_NOWAIT));
#endif
	if (va == NULL)
		return (ENOMEM);

	segs[0].ds_addr = va;
	segs[0].ds_len = size;
	*rsegs = 1;

#if 0
	/* XXX for now */
	for (epa = size + (spa = kvtop((caddr_t)va)); spa < epa; spa += NBPG)
		pmap_changebit(spa, TLB_UNCACHEABLE, 0);
#endif

	return 0;

}

void
_dmamem_free(v, segs, nsegs)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
{
#if defined (UVM)
	uvm_km_free(kmem_map, segs[0].ds_addr, M_DEVBUF);
#else
	kmem_free(kernel_map, segs[0].ds_addr, segs[0].ds_len);
#endif
}

int
_dmamem_map(v, segs, nsegs, size, kvap, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	*kvap = (caddr_t)segs[0].ds_addr;
	return 0;
}

void
_dmamem_unmap(v, kva, size)
	void *v;
	caddr_t kva;
	size_t size;
{
}

int
_dmamem_mmap(v, segs, nsegs, off, prot, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{
	panic("_dmamem_mmap: not implemented");
}

int
dma_cachectl(p, size)
	caddr_t p;
	int size;
{
#if 0
	fdcache(HPPA_SID_KERNEL, (vaddr_t)p, size);
	sync_caches();
#endif
	return 0;
}
