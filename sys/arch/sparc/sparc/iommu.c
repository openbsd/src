/*	$OpenBSD: iommu.c,v 1.28 2014/11/16 12:30:58 deraadt Exp $	*/
/*	$NetBSD: iommu.c,v 1.13 1997/07/29 09:42:04 fair Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1995 	Paul Kranenburg
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/mbuf.h>

#include <uvm/uvm_extern.h>

#include <machine/pmap.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/iommureg.h>

struct iommu_softc {
	struct device	sc_dev;		/* base device */
	struct iommureg	*sc_reg;
	u_int		sc_pagesize;
	u_int		sc_range;
	u_int		sc_dvmabase;
	iopte_t		*sc_ptes;
	int		sc_hasiocache;
#define sc_cachecoherent sc_hasiocache

/*
 * Note: operations on the extent map are being protected with
 * splhigh(), since we cannot predict at which interrupt priority
 * our clients will run.
 */
	struct sparc_bus_dma_tag sc_dmatag;
	struct extent *sc_dvmamap;
};

struct	iommu_softc *iommu_sc;/*XXX*/
struct sparc_bus_dma_tag *iommu_dmatag;/*XXX*/
int	has_iocache;

/* autoconfiguration driver */
int	iommu_print(void *, const char *);
void	iommu_attach(struct device *, struct device *, void *);
int	iommu_match(struct device *, void *, void *);

struct cfattach iommu_ca = {
	sizeof(struct iommu_softc), iommu_match, iommu_attach
};

struct cfdriver iommu_cd = {
	NULL, "iommu", DV_DULL
};

/* IOMMU DMA map functions */
int	iommu_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
			bus_size_t, int, bus_dmamap_t *);
int	iommu_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
			bus_size_t, struct proc *, int);
int	iommu_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
			struct mbuf *, int);
int	iommu_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
			struct uio *, int);
int	iommu_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
			bus_dma_segment_t *, int, bus_size_t, int);
void	iommu_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	iommu_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
			bus_size_t, int);

int	iommu_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *,
			int, size_t, caddr_t *, int);
void	iommu_dmamem_unmap(bus_dma_tag_t, void *, size_t);
paddr_t	iommu_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *,
			int, off_t, int, int);
int	iommu_dvma_alloc(struct iommu_softc *, bus_dmamap_t, vaddr_t,
			 bus_size_t, int, bus_addr_t *, bus_size_t *);

int	iommu_dmamap_load_buffer(bus_dma_tag_t, bus_dmamap_t, void *,
				 bus_size_t, struct proc *, int);

/*
 * Print the location of some iommu-attached device (called just
 * before attaching that device).  If `iommu' is not NULL, the
 * device was found but not configured; print the iommu as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
iommu_print(args, iommu)
	void *args;
	const char *iommu;
{
	register struct confargs *ca = args;

	if (iommu)
		printf("%s at %s", ca->ca_ra.ra_name, iommu);
	return (UNCONF);
}

int
iommu_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4OR4COR4E)
		return (0);
	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach the iommu.
 */
void
iommu_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
#if defined(SUN4M)
	struct iommu_softc *sc = (struct iommu_softc *)self;
	struct confargs oca, *ca = aux;
	struct sparc_bus_dma_tag *dmat = &sc->sc_dmatag;
	register struct romaux *ra = &ca->ca_ra;
	register int node;
	register char *name;
	register u_int pbase, pa;
	register int i, mmupcrsave, s;
	register iopte_t *tpte_p;
	struct pglist mlist;
	struct vm_page *m;
	vaddr_t va;
	paddr_t iopte_pa;

	iommu_sc = sc;
	iommu_dmatag = dmat;
	/*
	 * XXX there is only one iommu, for now -- do not know how to
	 * address children on others
	 */
	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	dmat->_cookie = sc;
	dmat->_dmamap_create = iommu_dmamap_create;
	dmat->_dmamap_destroy = _bus_dmamap_destroy;
	dmat->_dmamap_load = iommu_dmamap_load;
	dmat->_dmamap_load_mbuf = iommu_dmamap_load_mbuf;
	dmat->_dmamap_load_uio = iommu_dmamap_load_uio;
	dmat->_dmamap_load_raw = iommu_dmamap_load_raw;
	dmat->_dmamap_unload = iommu_dmamap_unload;
	dmat->_dmamap_sync = iommu_dmamap_sync;

	dmat->_dmamem_alloc = _bus_dmamem_alloc;
	dmat->_dmamem_free = _bus_dmamem_free;
	dmat->_dmamem_map = iommu_dmamem_map;
	dmat->_dmamem_unmap = _bus_dmamem_unmap;
	dmat->_dmamem_mmap = iommu_dmamem_mmap;

	node = ra->ra_node;

#if 0
	if (ra->ra_vaddr)
		sc->sc_reg = (struct iommureg *)ca->ca_ra.ra_vaddr;
#else
	/*
	 * Map registers into our space. The PROM may have done this
	 * already, but I feel better if we have our own copy. Plus, the
	 * prom doesn't map the entire register set
	 *
	 * XXX struct iommureg is bigger than ra->ra_len; what are the
	 *     other fields for?
	 */
	sc->sc_reg = (struct iommureg *)
		mapiodev(ra->ra_reg, 0, ra->ra_len);
#endif

	sc->sc_hasiocache = node_has_property(node, "cache-coherence?");
	if (CACHEINFO.c_enabled == 0) /* XXX - is this correct? */
		sc->sc_hasiocache = 0;
	has_iocache = sc->sc_hasiocache; /* Set global flag */

	sc->sc_pagesize = getpropint(node, "page-size", NBPG),
	sc->sc_range = (1 << 24) <<
	    ((sc->sc_reg->io_cr & IOMMU_CTL_RANGE) >> IOMMU_CTL_RANGESHFT);
#if 0
	sc->sc_dvmabase = (0 - sc->sc_range);
#endif
	pbase = (sc->sc_reg->io_bar & IOMMU_BAR_IBA) <<
			(14 - IOMMU_BAR_IBASHFT);

	/*
	 * Allocate memory for I/O pagetables. This takes 64k of memory
	 * since we want to have 64M of dvma space (this actually depends
	 * on the definition of DVMA4M_BASE...we may drop it back to 32M).
	 * The table must be aligned on a (-DVMA4M_BASE/NBPG) boundary
	 * (i.e. 64K for 64M of dvma space).
	 */
	TAILQ_INIT(&mlist);
#define DVMA_PTESIZE ((0 - DVMA4M_BASE) / 1024)
	if (uvm_pglistalloc(DVMA_PTESIZE, 0, 0xffffffff, DVMA_PTESIZE,
			    0, &mlist, 1, UVM_PLA_NOWAIT) ||
	    (va = uvm_km_valloc(kernel_map, DVMA_PTESIZE)) == 0)
		panic("iommu_attach: can't allocate memory for pagetables");
#undef DVMA_PTESIZE
	m = TAILQ_FIRST(&mlist);
	iopte_pa = VM_PAGE_TO_PHYS(m);
	sc->sc_ptes = (iopte_t *) va;

	while (m) {
		paddr_t pa = VM_PAGE_TO_PHYS(m);
		pmap_kenter_pa(va, pa | PMAP_NC, PROT_READ | PROT_WRITE);
		va += PAGE_SIZE;
		m = TAILQ_NEXT(m, pageq);
	}
	pmap_update(pmap_kernel());

	/*
	 * Now we build our own copy of the IOMMU page tables. We need to
	 * do this since we're going to change the range to give us 64M of
	 * mappings, and thus we can move DVMA space down to 0xfd000000 to
	 * give us lots of space and to avoid bumping into the PROM, etc.
	 *
	 * XXX Note that this is rather messy.
	 */

	/*
	 * Ok. We've got to read in the original table using MMU bypass,
	 * and copy all of its entries to the appropriate place in our
	 * new table, even if the sizes are different.
	 * This is pretty easy since we know DVMA ends at 0xffffffff.
	 *
	 * XXX: PGOFSET, NBPG assume same page size as SRMMU
	 */
	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		/* set MMU AC bit */
		sta(SRMMU_PCR, ASI_SRMMU,
		    ((mmupcrsave = lda(SRMMU_PCR, ASI_SRMMU)) | VIKING_PCR_AC));
	}

	for (tpte_p = &sc->sc_ptes[((0 - DVMA4M_BASE)/NBPG) - 1],
	     pa = (u_int)pbase - sizeof(iopte_t) +
		   ((u_int)sc->sc_range/NBPG)*sizeof(iopte_t);
	     tpte_p >= &sc->sc_ptes[0] && pa >= (u_int)pbase;
	     tpte_p--, pa -= sizeof(iopte_t)) {

		IOMMU_FLUSHPAGE(sc,
			        (tpte_p - &sc->sc_ptes[0])*NBPG + DVMA4M_BASE);
		*tpte_p = lda(pa, ASI_BYPASS);
	}
	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		/* restore mmu after bug-avoidance */
		sta(SRMMU_PCR, ASI_SRMMU, mmupcrsave);
	}

	/*
	 * Now we can install our new pagetable into the IOMMU
	 */
	sc->sc_range = 0 - DVMA4M_BASE;
	sc->sc_dvmabase = DVMA4M_BASE;

	/* calculate log2(sc->sc_range/16MB) */
	i = ffs(sc->sc_range/(1 << 24)) - 1;
	if ((1 << i) != (sc->sc_range/(1 << 24)))
		panic("bad iommu range: %d",i);

	s = splhigh();
	IOMMU_FLUSHALL(sc);

	sc->sc_reg->io_cr = (sc->sc_reg->io_cr & ~IOMMU_CTL_RANGE) |
			  (i << IOMMU_CTL_RANGESHFT) | IOMMU_CTL_ME;
	sc->sc_reg->io_bar = (iopte_pa >> 4) & IOMMU_BAR_IBA;

	IOMMU_FLUSHALL(sc);
	splx(s);

	printf(": version 0x%x/0x%x, page-size %d, range %dMB\n",
		(sc->sc_reg->io_cr & IOMMU_CTL_VER) >> 24,
		(sc->sc_reg->io_cr & IOMMU_CTL_IMPL) >> 28,
		sc->sc_pagesize,
		sc->sc_range >> 20);

	sc->sc_dvmamap = dvmamap_extent; /* XXX */

	/* Propagate bootpath */
	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "iommu") == 0)
		oca.ca_ra.ra_bp = ra->ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	/*
	 * Loop through ROM children (expect SBus among them).
	 */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;
		oca.ca_bustype = BUS_MAIN; /* ??? */
		(void) config_found(&sc->sc_dev, (void *)&oca, iommu_print);
	}
#endif
}

void
iommu_enter(va, pa)
	u_int va, pa;
{
	struct iommu_softc *sc = iommu_sc;
	int pte;

#ifdef DEBUG
	if (va < sc->sc_dvmabase)
		panic("iommu_enter: va 0x%x not in DVMA space",va);
#endif

	pte = atop(pa) << IOPTE_PPNSHFT;
	pte &= IOPTE_PPN;
	pte |= IOPTE_V | IOPTE_W | (has_iocache ? IOPTE_C : 0);
	sc->sc_ptes[atop(va - sc->sc_dvmabase)] = pte;
	IOMMU_FLUSHPAGE(sc, va);
}

/*
 * iommu_remove: clears mappings created by iommu_enter
 */
void
iommu_remove(va, len)
	register u_int va, len;
{
	register struct iommu_softc *sc = iommu_sc;

#ifdef DEBUG
	if (va < sc->sc_dvmabase)
		panic("iommu_enter: va 0x%x not in DVMA space", va);
#endif

	while (len > 0) {
#ifdef notyet
#ifdef DEBUG
		if ((sc->sc_ptes[atop(va - sc->sc_dvmabase)] & IOPTE_V) == 0)
			panic("iommu_remove: clearing invalid pte at va 0x%x",
				va);
#endif
#endif
		sc->sc_ptes[atop(va - sc->sc_dvmabase)] = 0;
		IOMMU_FLUSHPAGE(sc, va);
		len -= sc->sc_pagesize;
		va += sc->sc_pagesize;
	}
}

extern u_long dvma_cachealign;

/*
 * IOMMU DMA map functions.
 */
int
iommu_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
		    bus_size_t maxsegsz, bus_size_t boundary, int flags,
		    bus_dmamap_t *dmamp)
{
	struct iommu_softc *sc = t->_cookie;
	bus_dmamap_t map;
	int error;

	if ((error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
					boundary, flags, &map)) != 0)
		return (error);

	if ((flags & BUS_DMA_24BIT) != 0) {
		/* Limit this map to the range usable by `24-bit' devices */
		map->_dm_ex_start = DVMA_D24_BASE;
		map->_dm_ex_end = DVMA_D24_END;
	} else {
		/* Enable allocations from the entire map */
		map->_dm_ex_start = sc->sc_dvmamap->ex_start;
		map->_dm_ex_end = sc->sc_dvmamap->ex_end;
	}

	*dmamp = map;
	return (0);
}

/*
 * Internal routine to allocate space in the IOMMU map.
 */
int
iommu_dvma_alloc(struct iommu_softc *sc, bus_dmamap_t map,
		 vaddr_t va, bus_size_t len, int flags,
		 bus_addr_t *dvap, bus_size_t *sgsizep)
{
	bus_size_t sgsize;
	u_long align, voff, dvaddr;
	int s, error;
	int pagesz = PAGE_SIZE;

	/*
	 * Remember page offset, then truncate the buffer address to
	 * a page boundary.
	 */
	voff = va & (pagesz - 1);
	va &= -pagesz;

	if (len > map->_dm_size)
		return (EINVAL);

	sgsize = (len + voff + pagesz - 1) & -pagesz;
	align = dvma_cachealign ? dvma_cachealign : map->_dm_align;

	s = splhigh();
	error = extent_alloc_subregion(sc->sc_dvmamap, map->_dm_ex_start,
	    map->_dm_ex_end, sgsize, align, va & (align-1), map->_dm_boundary,
	    (flags & BUS_DMA_NOWAIT) == 0 ? EX_WAITOK : EX_NOWAIT, &dvaddr);
	splx(s);
	*dvap = (bus_addr_t)dvaddr;
	*sgsizep = sgsize;
	return (error);
}

int
iommu_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct iommu_softc *sc = t->_cookie;
	bus_size_t sgsize;
	bus_addr_t dva;
	vaddr_t va = (vaddr_t)buf;
	int pagesz = PAGE_SIZE;
	pmap_t pmap;
	int error;

	if (map->dm_nsegs >= map->_dm_segcnt)
		return (EFBIG);

	/* Allocate IOMMU resources */
	if ((error = iommu_dvma_alloc(sc, map, va, buflen, flags,
					&dva, &sgsize)) != 0)
		return (error);

	if ((sc->sc_cachecoherent == 0) || 
	    (CACHEINFO.ec_totalsize == 0))
		cpuinfo.cache_flush(buf, buflen); /* XXX - move to bus_dma_sync? */

	/*
	 * We always use just one segment.
	 */
	map->dm_segs[map->dm_nsegs].ds_addr = dva + (va & (pagesz - 1));
	map->dm_segs[map->dm_nsegs].ds_len = buflen;
	map->dm_segs[map->dm_nsegs]._ds_sgsize = sgsize;
	map->dm_nsegs++;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	for (; sgsize != 0; ) {
		paddr_t pa;
		/*
		 * Get the physical address for this page.
		 */
		if (!pmap_extract(pmap, va, &pa))
			return (EFAULT);

		iommu_enter(dva, pa);

		dva += pagesz;
		va += pagesz;
		sgsize -= pagesz;
	}

	return (0);
}

/*
 * Prepare buffer for DMA transfer.
 */
int
iommu_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map,
		  void *buf, bus_size_t buflen,
		  struct proc *p, int flags)
{
	int error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 0;

	error = iommu_dmamap_load_buffer(t, map, buf, buflen, p, flags);
	if (error)
		iommu_dmamap_unload(t, map);

	return (error);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
iommu_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map,
		       struct mbuf *m0, int flags)
{
	struct mbuf *m;
	int error = 0;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = m0->m_pkthdr.len;
	map->dm_nsegs = 0;

	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		error = iommu_dmamap_load_buffer(t, map, m->m_data, m->m_len,
		    NULL, flags);
	}

	if (error)
		iommu_dmamap_unload(t, map);

	return (error);
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
iommu_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map,
		      struct uio *uio, int flags)
{

	panic("_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
iommu_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map,
		      bus_dma_segment_t *segs, int nsegs, bus_size_t size,
		      int flags)
{
	struct iommu_softc *sc = t->_cookie;
	struct vm_page *m;
	paddr_t pa;
	bus_addr_t dva;
	bus_size_t sgsize;
	struct pglist *mlist;
	int pagesz = PAGE_SIZE;
	int error;

	map->dm_nsegs = 0;

	/* Allocate IOMMU resources */
	if ((error = iommu_dvma_alloc(sc, map, segs[0]._ds_va, size,
				      flags, &dva, &sgsize)) != 0)
		return (error);

	/*
	 * Note DVMA address in case bus_dmamem_map() is called later.
	 * It can then insure cache coherency by choosing a KVA that
	 * is aligned to `ds_addr'.
	 */
	segs[0].ds_addr = dva;
	segs[0].ds_len = size;

	map->dm_segs[0].ds_addr = dva;
	map->dm_segs[0].ds_len = size;
	map->dm_segs[0]._ds_sgsize = sgsize;

	/* Map physical pages into IOMMU */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {
		if (sgsize == 0)
			panic("iommu_dmamap_load_raw: size botch");
		pa = VM_PAGE_TO_PHYS(m);
		iommu_enter(dva, pa);
		dva += pagesz;
		sgsize -= pagesz;
	}

	map->dm_nsegs = 1;
	map->dm_mapsize = size;

	return (0);
}

/*
 * Unload an IOMMU DMA map.
 */
void
iommu_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct iommu_softc *sc = t->_cookie;
	bus_dma_segment_t *segs = map->dm_segs;
	int nsegs = map->dm_nsegs;
	bus_addr_t dva;
	bus_size_t len;
	int i, s, error;

	for (i = 0; i < nsegs; i++) {
		dva = segs[i].ds_addr & -PAGE_SIZE;
		len = segs[i]._ds_sgsize;

		iommu_remove(dva, len);
		s = splhigh();
		error = extent_free(sc->sc_dvmamap, dva, len, EX_NOWAIT);
		splx(s);
		if (error != 0)
			printf("warning: %ld of DVMA space lost\n", (long)len);
	}

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

/*
 * DMA map synchronization.
 */
void
iommu_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map,
		  bus_addr_t offset, bus_size_t len, int ops)
{

	/*
	 * XXX Should flush CPU write buffers.
	 */
}

/*
 * Map DMA-safe memory.
 */
int
iommu_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
		 size_t size, caddr_t *kvap, int flags)
{
	struct iommu_softc *sc = t->_cookie;
	struct vm_page *m;
	vaddr_t va;
	bus_addr_t addr;
	struct pglist *mlist;
	int cbit;
	u_long align;
	int pagesz = PAGE_SIZE;
	const struct kmem_dyn_mode *kd;

	if (nsegs != 1)
		panic("iommu_dmamem_map: nsegs = %d", nsegs);

	cbit = sc->sc_cachecoherent ? 0 : PMAP_NC;
	align = dvma_cachealign ? dvma_cachealign : pagesz;

	size = round_page(size);

#if 0
	/*
	 * In case the segment has already been loaded by
	 * iommu_dmamap_load_raw(), find a region of kernel virtual
	 * addresses that can accommodate our alignment requirements.
	 */
	va = _bus_dma_valloc_skewed(size, 0, align,
				    segs[0].ds_addr & (align - 1));
#else
	kd = flags & BUS_DMA_NOWAIT ? &kd_trylock : &kd_waitok;
	va = (vaddr_t)km_alloc(size, &kv_any, &kp_none, kd);
#endif
	if (va == 0)
		return (ENOMEM);

	segs[0]._ds_va = va;
	*kvap = (void *)va;

	/*
	 * Map the pages allocated in _bus_dmamem_alloc() to the
	 * kernel virtual address space.
	 */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {

		if (size == 0)
			panic("iommu_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_kenter_pa(va, addr | cbit, PROT_READ | PROT_WRITE);
#if 0
			if (flags & BUS_DMA_COHERENT)
				/* XXX */;
#endif
		va += pagesz;
		size -= pagesz;
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
iommu_dmamem_unmap(bus_dma_tag_t t, void *kva, size_t size)
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PAGE_MASK)
		panic("iommu_dmamem_unmap");
#endif

	km_free(kva, round_page(size), &kv_any, &kp_none);
}


/*
 * mmap(2)'ing DMA-safe memory.
 */
paddr_t
iommu_dmamem_mmap(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
		  off_t off, int prot, int flags)
{
	panic("_bus_dmamem_mmap: not implemented");
}
