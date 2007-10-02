/*	$OpenBSD: mainbus.c,v 1.5 2007/10/02 00:59:12 krw Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "power.h"

#undef BTLBDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/extent.h>
#include <sys/mbuf.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

struct mainbus_softc {
	struct  device sc_dv;

	hppa_hpa_t sc_hpa;
};

int	mbmatch(struct device *, void *, void *);
void	mbattach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

struct pdc_hpa pdc_hpa PDC_ALIGNMENT;
struct pdc_power_info pdc_power_info PDC_ALIGNMENT;

/* from machdep.c */
extern struct extent *hppa_ex;

int
mbus_add_mapping(bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	paddr_t spa, epa;
	int bank, off;

	if ((bank = vm_physseg_find(atop(bpa), &off)) >= 0)
		panic("mbus_add_mapping: mapping real memory @0x%lx", bpa);

	for (spa = trunc_page(bpa), epa = bpa + size;
	     spa < epa; spa += PAGE_SIZE)
		pmap_kenter_pa(spa, spa, UVM_PROT_RW);

	*bshp = bpa;
	return (0);
}

int
mbus_map(void *v, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	int error;

	bpa &= HPPA_PHYSMAP;
	if ((error = extent_alloc_region(hppa_ex, bpa, size, EX_NOWAIT)))
		return (error);

	if ((error = mbus_add_mapping(bpa, size, flags, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT)) {
			printf("bus_space_map: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}

	return error;
}

void
mbus_unmap(void *v, bus_space_handle_t bsh, bus_size_t size)
{
	u_long sva, eva;

	sva = trunc_page(bsh);
	eva = round_page(bsh + size);

#ifdef DIAGNOSTIC
	if (eva <= sva)
		panic("bus_space_unmap: overflow");
#endif

	pmap_kremove(sva, eva - sva);

	if (extent_free(hppa_ex, bsh, size, EX_NOWAIT)) {
		printf("bus_space_unmap: ps 0x%lx, size 0x%lx\n",
		    bsh, size);
		printf("bus_space_unmap: can't free region\n");
	}
}

int
mbus_alloc(void *v, bus_addr_t rstart, bus_addr_t rend, bus_size_t size,
	 bus_size_t align, bus_size_t boundary, int flags,
	 bus_addr_t *addrp, bus_space_handle_t *bshp)
{
	bus_addr_t bpa;
	int error;

	rstart &= HPPA_PHYSMAP;
	rend &= HPPA_PHYSMAP;
	if (rstart < hppa_ex->ex_start || rend > hppa_ex->ex_end)
		panic("bus_space_alloc: bad region start/end");

	if ((error = extent_alloc_subregion(hppa_ex, rstart, rend, size,
	    align, 0, boundary, EX_NOWAIT, &bpa)))
		return (error);

	if ((error = mbus_add_mapping(bpa, size, flags, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT)) {
			printf("bus_space_alloc: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf("bus_space_alloc: can't free region\n");
		}
	}

	*addrp = bpa | ~HPPA_PHYSMAP;
	return (error);
}

void
mbus_free(void *v, bus_space_handle_t h, bus_size_t size)
{
	/* bus_space_unmap() does all that we need to do. */
	mbus_unmap(v, h, size);
}

int
mbus_subregion(void *v, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void
mbus_barrier(void *v, bus_space_handle_t h, bus_size_t o, bus_size_t l, int op)
{
	sync_caches();
}

struct hppa64_bus_space_tag hppa_bustag = {
	NULL,

	mbus_map, mbus_unmap, mbus_subregion, mbus_alloc, mbus_free,
	mbus_barrier,
};

int
mbus_dmamap_create(void *v, bus_size_t size, int nsegments,
		   bus_size_t maxsegsz, bus_size_t boundary, int flags,
		   bus_dmamap_t *dmamp)
{
	struct hppa64_bus_dmamap *map;
	size_t mapsize;

	mapsize = sizeof(*map) + (sizeof(bus_dma_segment_t) * (nsegments - 1));
	map = malloc(mapsize, M_DEVBUF, (flags & BUS_DMA_NOWAIT) ?
	    (M_NOWAIT | M_ZERO) : (M_WAITOK | M_ZERO));
	if (!map)
		return (ENOMEM);

	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

void
mbus_dmamap_unload(void *v, bus_dmamap_t map)
{
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

void
mbus_dmamap_destroy(void *v, bus_dmamap_t map)
{
	if (map->dm_mapsize != 0)
		mbus_dmamap_unload(v, map);

	free(map, M_DEVBUF);
}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
int
_bus_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags, paddr_t *lastaddrp,
    int *segp, int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vaddr_t vaddr = (vaddr_t)buf;
	int seg;
	pmap_t pmap;

	pmap = p? p->p_vmspace->vm_map.pmap : pmap_kernel();
	lastaddr = *lastaddrp;
	bmask  = ~(map->_dm_boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		pmap_extract(pmap, vaddr, (paddr_t *)&curaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)vaddr & PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = (curaddr + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = curaddr;
			map->dm_segs[seg].ds_len = sgsize;
			map->dm_segs[seg]._ds_va = vaddr;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz &&
			    (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     (curaddr & bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt)
					break;
				map->dm_segs[seg].ds_addr = curaddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_va = vaddr;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		return (EFBIG);		/* XXX better return value here? */
	return (0);
}

int
mbus_dmamap_load(void *v, bus_dmamap_t map, void *addr, bus_size_t size,
		 struct proc *p, int flags)
{
	paddr_t lastaddr;
	int seg, error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;

	if (size > map->_dm_size)
		return (EINVAL);

	seg = 0;
	lastaddr = 0;
	error = _bus_dmamap_load_buffer(NULL, map, addr, size, p, flags,
	    &lastaddr, &seg, 1);
	if (error == 0) {
		map->dm_mapsize = size;
		map->dm_nsegs = seg + 1;
	}

	return (0);
}

int
mbus_dmamap_load_mbuf(void *v, bus_dmamap_t map, struct mbuf *m0, int flags)
{
	paddr_t lastaddr;
	int seg, error, first;
	struct mbuf *m;

	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

#ifdef DIAGNOSTIC
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("_bus_dmamap_load_mbuf: no packet header");
#endif  

	if (m0->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	first = 1;
	seg = 0;
	error = 0;
	lastaddr = 0;
	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		error = _bus_dmamap_load_buffer(NULL, map, m->m_data, m->m_len,
		    NULL, flags, &lastaddr, &seg, first);
		first = 0;
	}
	if (error == 0) {
		map->dm_mapsize = m0->m_pkthdr.len;
		map->dm_nsegs = seg + 1;
	}

	return (error);
}

int
mbus_dmamap_load_uio(void *v, bus_dmamap_t map, struct uio *uio, int flags)
{
	paddr_t lastaddr;
	int seg, i, error, first;
	bus_size_t minlen, resid;
	struct proc *p = NULL;
	struct iovec *iov;
	caddr_t addr;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (resid > map->_dm_size)
		return (EINVAL);

	if (uio->uio_segflg == UIO_USERSPACE) {
		p = uio->uio_procp;
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("_bus_dmamap_load_uio: USERSPACE but no proc");
#endif
	}

	first = 1;
	seg = 0;
	error = 0;
	lastaddr = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && error == 0; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		addr = (caddr_t)iov[i].iov_base;

		error = _bus_dmamap_load_buffer(NULL, map, addr, minlen,
		    p, flags, &lastaddr, &seg, first);
		first = 0;

		resid -= minlen;
	}
	if (error == 0) {
		map->dm_mapsize = uio->uio_resid;
		map->dm_nsegs = seg + 1;
	}
	return (error);
}

int
mbus_dmamap_load_raw(void *v, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	if (nsegs > map->_dm_segcnt || size > map->_dm_size)
		return (EINVAL);

	/*
	 * Make sure we don't cross any boundaries.
	 */
	if (map->_dm_boundary) {
		bus_addr_t bmask = ~(map->_dm_boundary - 1);
		int i;

		for (i = 0; i < nsegs; i++) {
			if (segs[i].ds_len > map->_dm_maxsegsz)
				return (EINVAL);
			if ((segs[i].ds_addr & bmask) !=
			    ((segs[i].ds_addr + segs[i].ds_len - 1) & bmask))
				return (EINVAL);
		}
	}

	bcopy(segs, map->dm_segs, nsegs * sizeof(*segs));
	map->dm_nsegs = nsegs;
	map->dm_mapsize = size;
	return (0);
}

void
mbus_dmamap_sync(void *v, bus_dmamap_t map, bus_addr_t off, bus_size_t len,
    int ops)
{
	bus_dma_segment_t *ps = map->dm_segs,
	    *es = &map->dm_segs[map->dm_nsegs];

	if (off >= map->_dm_size)
		return;

	if ((off + len) > map->_dm_size)
		len = map->_dm_size - off;

	for (; len && ps < es; ps++)
		if (off > ps->ds_len)
			off -= ps->ds_len;
		else {
			bus_size_t l = ps->ds_len - off;
			if (l > len)
				l = len;
			fdcache(HPPA_SID_KERNEL, ps->_ds_va + off, l);
			len -= l;
			off = 0;
		}

	/* for either operation sync the shit away */
	sync_caches();
}

int
mbus_dmamem_alloc(void *v, bus_size_t size, bus_size_t alignment,
		  bus_size_t boundary, bus_dma_segment_t *segs, int nsegs,
		  int *rsegs, int flags)
{
	extern paddr_t avail_end;
	struct pglist pglist;
	struct vm_page *pg;

	size = round_page(size);

	TAILQ_INIT(&pglist);
	if (uvm_pglistalloc(size, 0, avail_end, alignment, boundary,
	    &pglist, 1, flags & BUS_DMA_NOWAIT))
		return (ENOMEM);

	pg = TAILQ_FIRST(&pglist);
	segs[0]._ds_va = segs[0].ds_addr = VM_PAGE_TO_PHYS(pg);
	segs[0].ds_len = size;
	*rsegs = 1;

	for(; pg; pg = TAILQ_NEXT(pg, pageq))
		/* XXX for now */
		pmap_changebit(pg, PTE_UNCACHABLE, 0);
	pmap_update(pmap_kernel());

	return (0);
}

void
mbus_dmamem_free(void *v, bus_dma_segment_t *segs, int nsegs)
{
	struct pglist pglist;
	paddr_t pa, epa;

	TAILQ_INIT(&pglist);
	for(; nsegs--; segs++)
		for (pa = segs->ds_addr, epa = pa + segs->ds_len;
		     pa < epa; pa += PAGE_SIZE) {
			struct vm_page *pg = PHYS_TO_VM_PAGE(pa);
			if (!pg)
				panic("mbus_dmamem_free: no page for pa");
			TAILQ_INSERT_TAIL(&pglist, pg, pageq);
		}
	uvm_pglistfree(&pglist);
}

int
mbus_dmamem_map(void *v, bus_dma_segment_t *segs, int nsegs, size_t size,
		caddr_t *kvap, int flags)
{
	*kvap = (caddr_t)segs[0].ds_addr;
	return 0;
}

void
mbus_dmamem_unmap(void *v, caddr_t kva, size_t size)
{
}

paddr_t
mbus_dmamem_mmap(void *v, bus_dma_segment_t *segs, int nsegs, off_t off,
		 int prot, int flags)
{
	panic("_dmamem_mmap: not implemented");
}

const struct hppa64_bus_dma_tag hppa_dmatag = {
	NULL,
	mbus_dmamap_create, mbus_dmamap_destroy,
	mbus_dmamap_load, mbus_dmamap_load_mbuf,
	mbus_dmamap_load_uio, mbus_dmamap_load_raw,
	mbus_dmamap_unload, mbus_dmamap_sync,

	mbus_dmamem_alloc, mbus_dmamem_free, mbus_dmamem_map,
	mbus_dmamem_unmap, mbus_dmamem_mmap
};

int
mbmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

	/* there will be only one */
	if (cf->cf_unit)
		return 0;

	return 1;
}

void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct confargs nca;
	bus_space_handle_t ioh;
	bus_addr_t hpa;

	/* fetch the "default" cpu hpa */
	if (pdc_call((iodcio_t)pdc, 0, PDC_HPA, PDC_HPA_DFLT, &pdc_hpa) < 0)
		panic("mbattach: PDC_HPA failed");
	hpa = pdc_hpa.hpa | 0xffffffff00000000UL;

	printf(" [flex %lx]\n", hpa & HPPA_FLEX_MASK);

	/* map all the way till the end of the memory */
	if (bus_space_map(&hppa_bustag, hpa, HPPA_PHYSEND - hpa + 1, 0, &ioh))
		panic("mbattach: cannot map mainbus IO space");

	/*
	 * Local-Broadcast the HPA to all modules on this bus
	 */
	((struct iomod *)(HPPA_LBCAST & HPPA_PHYSMAP))->io_flex =
	    (hpa & HPPA_FLEX_MASK) | DMA_ENABLE;

	sc->sc_hpa = hpa;

	/* PDC first */
	bzero (&nca, sizeof(nca));
	nca.ca_name = "pdc";
	nca.ca_iot = &hppa_bustag;
	nca.ca_dmatag = &hppa_dmatag;
	config_found(self, &nca, mbprint);

#if NPOWER > 0
	/* get some power */
	bzero (&nca, sizeof(nca));
	nca.ca_name = "power";
	nca.ca_irq = -1;
	if (!pdc_call((iodcio_t)pdc, 0, PDC_SOFT_POWER,
	    PDC_SOFT_POWER_INFO, &pdc_power_info, 0)) {
		nca.ca_iot = &hppa_bustag;
		nca.ca_hpa = pdc_power_info.addr;
		nca.ca_hpamask = HPPA_IOBEGIN;
	}
	config_found(self, &nca, mbprint);
#endif

	bzero (&nca, sizeof(nca));
	nca.ca_iot = &hppa_bustag;
	nca.ca_dmatag = &hppa_dmatag;
	nca.ca_mod = -1;
	pdc_scan(self, &nca);
}

/*
 * retrive CPU #N HPA value
 */
hppa_hpa_t
cpu_gethpa(n)
	int n;
{
	struct mainbus_softc *sc;

	sc = mainbus_cd.cd_devs[0];

	return sc->sc_hpa;
}

int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("\"%s\" at %s", ca->ca_name, pnp);

	if (ca->ca_hpa)
		printf(" hpa %lx", ca->ca_hpa);

	return (UNCONF);
}
