/*	$OpenBSD: iommu.c,v 1.16 2005/09/29 21:30:42 marco Exp $	*/

/*
 * Copyright (c) 2005 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 *	- map the PTE uncacheable and disable table walk probes
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/lock.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#define _X86_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/pio.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define	MCANB_CTRL	0x40		/* Machine Check, NorthBridge */
#define	SCRUB_CTRL	0x58		/* dram/l2/dcache */
#define	GART_APCTRL	0x90		/* aperture control */
#define	GART_APBASE	0x94		/* aperture base */
#define	GART_TBLBASE	0x98		/* aperture table base */
#define	GART_CACHECTRL	0x9c		/* aperture cache control */

#define	MCANB_CORRECCEN		0x00000001	/* correctable ecc error */
#define	MCANB_UNCORRECCEN	0x00000002	/* uncorrectable ecc error */
#define	MCANB_CRCERR0EN		0x00000004	/* hypertrans link 0 crc */
#define	MCANB_CRCERR1EN		0x00000008	/* hypertrans link 1 crc */
#define	MCANB_CRCERR2EN		0x00000010	/* hypertrans link 2 crc */
#define	MCANB_SYNCPKT0EN	0x00000020	/* hypertrans link 0 sync */
#define	MCANB_SYNCPKT1EN	0x00000040	/* hypertrans link 1 sync */
#define	MCANB_SYNCPKT2EN	0x00000080	/* hypertrans link 2 sync */
#define	MCANB_MSTRABRTEN	0x00000100	/* master abort error */
#define	MCANB_TGTABRTEN		0x00000200	/* target abort error */
#define	MCANB_GARTTBLWKEN	0x00000400	/* gart table walk error */
#define	MCANB_ATOMICRMWEN	0x00000800	/* atomic r/m/w error */
#define	MCANB_WCHDOGTMREN	0x00001000	/* watchdog timer error */

#define	GART_APCTRL_ENABLE	0x00000001	/* enable */
#define	GART_APCTRL_SIZE	0x0000000e	/* size mask */
#define	GART_APCTRL_SIZE_32M	0x00000000	/*  32M */
#define	GART_APCTRL_SIZE_64M	0x00000002	/*  64M */
#define	GART_APCTRL_SIZE_128M	0x00000004	/*  128M */
#define	GART_APCTRL_SIZE_256M	0x00000006	/*  256M */
#define	GART_APCTRL_SIZE_512M	0x00000008	/*  512M */
#define	GART_APCTRL_SIZE_1G	0x0000000a	/*  1G */
#define	GART_APCTRL_SIZE_2G	0x0000000c	/*  2G */
#define	GART_APCTRL_DISCPU	0x00000010	/* disable CPU access */
#define	GART_APCTRL_DISIO	0x00000020	/* disable IO access */
#define	GART_APCTRL_DISTBL	0x00000040	/* disable table walk probe */

#define	GART_APBASE_MASK	0x00007fff	/* base [39:25] */

#define	GART_TBLBASE_MASK	0xfffffff0	/* table base [39:12] */

#define	GART_PTE_VALID		0x00000001	/* valid */
#define	GART_PTE_COHERENT	0x00000002	/* coherent */
#define	GART_PTE_PHYSHI		0x00000ff0	/* phys addr[39:32] */
#define	GART_PTE_PHYSLO		0xfffff000	/* phys addr[31:12] */

#define	GART_CACHE_INVALIDATE	0x00000001	/* invalidate (s/c) */
#define	GART_CACHE_PTEERR	0x00000002	/* pte error */

#define	IOMMU_START		0x80000000	/* beginning */
#define	IOMMU_END		0xffffffff	/* end */
#define	IOMMU_SIZE		512		/* size in MB */
#define	IOMMU_ALIGN		IOMMU_SIZE

extern paddr_t avail_end;
extern struct extent *iomem_ex;

int amdgarts;
int amdgart_enable = 0;

struct amdgart_softc {
	pci_chipset_tag_t g_pc;
	pcitag_t g_tag;
	struct extent *g_ex;
	paddr_t g_pa;
	paddr_t g_scribpa;
	void *g_scrib;
	u_int32_t g_scribpte;
	u_int32_t *g_pte;
	bus_dma_tag_t g_dmat;
} *amdgart_softcs;

void amdgart_invalidate_wait(void);
void amdgart_invalidate(void);
void amdgart_probe(struct pcibus_attach_args *);
void amdgart_dumpregs(void);
int amdgart_iommu_map(bus_dmamap_t, struct extent *, paddr_t,
    paddr_t *, psize_t);
int amdgart_iommu_unmap(struct extent *, paddr_t, psize_t);
int amdgart_reload(struct extent *, bus_dmamap_t);
int amdgart_ok(pci_chipset_tag_t, pcitag_t);
void amdgart_initpt(struct amdgart_softc *, u_long);

int amdgart_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
    bus_size_t, int, bus_dmamap_t *);
void amdgart_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int amdgart_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    struct proc *, int);
int amdgart_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *, int);
int amdgart_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t, struct uio *, int);
int amdgart_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
    bus_dma_segment_t *, int, bus_size_t, int);
void amdgart_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void amdgart_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    bus_size_t, int);

int amdgart_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
void amdgart_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int amdgart_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
void amdgart_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
paddr_t amdgart_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int, off_t,
    int, int);

struct x86_bus_dma_tag amdgart_bus_dma_tag = {
	NULL,			/* _may_bounce */
	amdgart_dmamap_create,
	amdgart_dmamap_destroy,
	amdgart_dmamap_load,
	amdgart_dmamap_load_mbuf,
	amdgart_dmamap_load_uio,
	amdgart_dmamap_load_raw,
	amdgart_dmamap_unload,
	NULL,
	amdgart_dmamem_alloc,
	amdgart_dmamem_free,
	amdgart_dmamem_map,
	amdgart_dmamem_unmap,
	amdgart_dmamem_mmap,
};

void
amdgart_invalidate_wait(void)
{
	u_int32_t v;
	int i, n;

	for (n = 0; n < amdgarts; n++) {
		for (i = 1000; i > 0; i--) {
			v = pci_conf_read(amdgart_softcs[n].g_pc,
			    amdgart_softcs[n].g_tag, GART_CACHECTRL);
			if ((v & GART_CACHE_INVALIDATE) == 0)
				break;
			delay(1);
		}
		if (i == 0)
			printf("GART%d: timeout\n", n);
	}
}

void
amdgart_invalidate(void)
{
	int n;

	for (n = 0; n < amdgarts; n++)
		pci_conf_write(amdgart_softcs[n].g_pc,
		    amdgart_softcs[n].g_tag, GART_CACHECTRL,
		    GART_CACHE_INVALIDATE);
}

void
amdgart_dumpregs(void)
{
	int n, i, dirty;
	u_int8_t *p;

	for (n = 0; n < amdgarts; n++) {
		printf("GART%d:\n", n);
		printf(" apctl %x\n", pci_conf_read(amdgart_softcs[n].g_pc,
		    amdgart_softcs[n].g_tag, GART_APCTRL));
		printf(" apbase %x\n", pci_conf_read(amdgart_softcs[n].g_pc,
		    amdgart_softcs[n].g_tag, GART_APBASE));
		printf(" tblbase %x\n", pci_conf_read(amdgart_softcs[n].g_pc,
		    amdgart_softcs[n].g_tag, GART_TBLBASE));
		printf(" cachectl %x\n", pci_conf_read(amdgart_softcs[n].g_pc,
		    amdgart_softcs[n].g_tag, GART_CACHECTRL));

		p = amdgart_softcs[n].g_scrib;
		dirty = 0;
		for (i = 0; i < PAGE_SIZE; i++, p++)
			if (*p != '\0')
				dirty++;
		printf(" scribble: %s\n", dirty ? "dirty" : "clean");
	}
}

int
amdgart_ok(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t v;

	v = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(v) != PCI_VENDOR_AMD)
		return (0);
	if (PCI_PRODUCT(v) != PCI_PRODUCT_AMD_AMD64_MISC)
		return (0);

	v = pci_conf_read(pc, tag, GART_APCTRL);
	if (v & GART_APCTRL_ENABLE)
		return (0);

	return (1);
}

void
amdgart_probe(struct pcibus_attach_args *pba)
{
	int dev, func, count = 0, r;
	u_long dvabase = (u_long)-1, mapsize, ptesize;
	pcitag_t tag;
	pcireg_t v;
	struct pglist plist;
	void *scrib = NULL;
	struct extent *ex = NULL;
	u_int32_t *pte;
	paddr_t ptepa;

	if (amdgart_enable == 0)
		return;

	TAILQ_INIT(&plist);

	for (count = 0, dev = 24; dev < 32; dev++) {
		for (func = 0; func < 8; func++) {
			tag = pci_make_tag(pba->pba_pc, 0, dev, func);

			if (amdgart_ok(pba->pba_pc, tag))
				count++;
		}
	}

	if (count == 0)
		return;

	amdgart_softcs = malloc(sizeof(*amdgart_softcs) * count, M_DEVBUF,
	    M_NOWAIT);
	if (amdgart_softcs == NULL) {
		printf("\nGART: can't get softc");
		goto err;
	}

	dvabase = IOMMU_START;

	mapsize = IOMMU_SIZE * 1024 * 1024;
	ptesize = mapsize / (PAGE_SIZE / sizeof(u_int32_t));

	r = uvm_pglistalloc(ptesize, ptesize, trunc_page(avail_end),
	    ptesize, ptesize, &plist, 1, 0);
	if (r != 0) {
		printf("\nGART: failed to get pte pages");
		goto err;
	}
	ptepa = VM_PAGE_TO_PHYS(TAILQ_FIRST(&plist));
	pte = (u_int32_t *)pmap_map_direct(TAILQ_FIRST(&plist));

	ex = extent_create("iommu", dvabase, dvabase + mapsize - 1, M_DEVBUF,
	    NULL, NULL, EX_NOWAIT | EX_NOCOALESCE);
	if (ex == NULL) {
		printf("\nGART: extent create failed");
		goto err;
	}

	scrib = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (scrib == NULL) {
		printf("\nGART: didn't get scribble page");
		goto err;
	}
	bzero(scrib, PAGE_SIZE);

	for (count = 0, dev = 24; dev < 32; dev++) {
		for (func = 0; func < 8; func++) {
			tag = pci_make_tag(pba->pba_pc, 0, dev, func);

			if (!amdgart_ok(pba->pba_pc, tag))
				continue;

			v = pci_conf_read(pba->pba_pc, tag, GART_APCTRL);
			v |= GART_APCTRL_DISCPU | GART_APCTRL_DISTBL |
			    GART_APCTRL_DISIO;
			v &= ~(GART_APCTRL_ENABLE | GART_APCTRL_SIZE);
			switch (IOMMU_SIZE) {
			case 32:
				v |= GART_APCTRL_SIZE_32M;
				break;
			case 64:
				v |= GART_APCTRL_SIZE_64M;
				break;
			case 128:
				v |= GART_APCTRL_SIZE_128M;
				break;
			case 256:
				v |= GART_APCTRL_SIZE_256M;
				break;
			case 512:
				v |= GART_APCTRL_SIZE_512M;
				break;
			case 1024:
				v |= GART_APCTRL_SIZE_1G;
				break;
			case 2048:
				v |= GART_APCTRL_SIZE_2G;
				break;
			default:
				printf("\nGART: bad size");
				return;
			}
			pci_conf_write(pba->pba_pc, tag, GART_APCTRL, v);

			pci_conf_write(pba->pba_pc, tag, GART_APBASE,
			    dvabase >> 25);

			pci_conf_write(pba->pba_pc, tag, GART_TBLBASE,
			    (ptepa >> 8) & GART_TBLBASE_MASK);

			v = pci_conf_read(pba->pba_pc, tag, GART_APCTRL);
			v |= GART_APCTRL_ENABLE;
			v &= ~(GART_APCTRL_DISIO | GART_APCTRL_DISTBL);
			pci_conf_write(pba->pba_pc, tag, GART_APCTRL, v);

			amdgart_softcs[count].g_pc = pba->pba_pc;
			amdgart_softcs[count].g_tag = tag;
			amdgart_softcs[count].g_ex = ex;
			amdgart_softcs[count].g_pa = dvabase;
			pmap_extract(pmap_kernel(), (vaddr_t)scrib,
			    &amdgart_softcs[count].g_scribpa);
			amdgart_softcs[count].g_scrib = scrib;
			amdgart_softcs[count].g_scribpte =
			    GART_PTE_VALID | GART_PTE_COHERENT |
			    ((amdgart_softcs[count].g_scribpa >> 28) &
			      GART_PTE_PHYSHI) |
			     (amdgart_softcs[count].g_scribpa &
			      GART_PTE_PHYSLO);
			amdgart_softcs[count].g_pte = pte;
			amdgart_softcs[count].g_dmat = pba->pba_dmat;

			amdgart_initpt(&amdgart_softcs[count],
			    ptesize / sizeof(*amdgart_softcs[count].g_pte));

			printf("\niommu%d at cpu%d: base 0x%lx length %dMB pte 0x%lx",
			    count, dev - 24, dvabase, IOMMU_SIZE, ptepa);
			count++;
		}
	}

	pba->pba_dmat = &amdgart_bus_dma_tag;
	amdgarts = count;

	return;

err:
	if (ex != NULL)
		extent_destroy(ex);
	if (scrib != NULL)
		free(scrib, M_DEVBUF);
	if (amdgart_softcs != NULL)
		free(amdgart_softcs, M_DEVBUF);
	if (!TAILQ_EMPTY(&plist))
		uvm_pglistfree(&plist);
}

void
amdgart_initpt(struct amdgart_softc *sc, u_long nent)
{
	u_long i;

	for (i = 0; i < nent; i++)
		sc->g_pte[i] = sc->g_scribpte;
	amdgart_invalidate();
	amdgart_invalidate_wait();
}

int
amdgart_reload(struct extent *ex, bus_dmamap_t dmam)
{
	int i, j, err;

	for (i = 0; i < dmam->dm_nsegs; i++) {
		paddr_t opa, npa;
		psize_t len;

		opa = dmam->dm_segs[i].ds_addr;
		len = dmam->dm_segs[i].ds_len;
		err = amdgart_iommu_map(dmam, ex, opa, &npa, len);
		if (err) {
			for (j = 0; j < i - 1; j++)
				amdgart_iommu_unmap(ex,
				    dmam->dm_segs[j].ds_addr,
				    dmam->dm_segs[j].ds_len);
			return (err);
		}
		dmam->dm_segs[i].ds_addr = npa;
	}
	return (0);
}

int
amdgart_iommu_map(bus_dmamap_t dmam, struct extent *ex, paddr_t opa,
    paddr_t *npa, psize_t len)
{
	paddr_t base, end, idx;
	psize_t alen;
	u_long res;
	int err, s;
	u_int32_t pgno, flags;

	base = trunc_page(opa);
	end = roundup(opa + len, PAGE_SIZE);
	alen = end - base;
	s = splhigh();
	err = extent_alloc(ex, alen, PAGE_SIZE, 0, dmam->_dm_boundary,
	    EX_NOWAIT, &res);
	splx(s);
	if (err) {
		printf("GART: extent_alloc %d\n", err);
		return (err);
	}
	*npa = res | (opa & PGOFSET);

	for (idx = 0; idx < alen; idx += PAGE_SIZE) {
		pgno = ((res + idx) - amdgart_softcs[0].g_pa) >> PGSHIFT;
		flags = GART_PTE_VALID | GART_PTE_COHERENT |
		    (((base + idx) >> 28) & GART_PTE_PHYSHI) |
		     ((base + idx) & GART_PTE_PHYSLO);
		amdgart_softcs[0].g_pte[pgno] = flags;
	}

	return (0);
}

int
amdgart_iommu_unmap(struct extent *ex, paddr_t pa, psize_t len)
{
	paddr_t base, end, idx;
	psize_t alen;
	int err, s;
	u_int32_t pgno;

	base = trunc_page(pa);
	end = roundup(pa + len, PAGE_SIZE);
	alen = end - base;

	/*
	 * order is significant here; invalidate the iommu page table
	 * entries, then mark them as freed in the extent.
	 */

	for (idx = 0; idx < alen; idx += PAGE_SIZE) {
		pgno = ((base - amdgart_softcs[0].g_pa) + idx) >> PGSHIFT;
		amdgart_softcs[0].g_pte[pgno] = 0;
	}

	s = splhigh();
	err = extent_free(ex, base, alen, EX_NOWAIT);
	splx(s);
	if (err) {
		/* XXX Shouldn't happen, but if it does, I think we lose. */
		printf("GART: extent_free %d\n", err);
		return (err);
	}

	return (0);
}

int
amdgart_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	return (bus_dmamap_create(amdgart_softcs[0].g_dmat, size, nsegments,
	    maxsegsz, boundary, flags, dmamp));
}

void
amdgart_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	bus_dmamap_destroy(amdgart_softcs[0].g_dmat, dmam);
}

int
amdgart_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	int err;

	err = bus_dmamap_load(amdgart_softcs[0].g_dmat, dmam, buf, buflen,
	    p, flags);
	if (err)
		return (err);
	err = amdgart_reload(amdgart_softcs[0].g_ex, dmam);
	if (err)
		bus_dmamap_unload(amdgart_softcs[0].g_dmat, dmam);
	else
		amdgart_invalidate();
	return (err);
}

int
amdgart_dmamap_load_mbuf(bus_dma_tag_t tag, bus_dmamap_t dmam,
    struct mbuf *chain, int flags)
{
	int err;

	err = bus_dmamap_load_mbuf(amdgart_softcs[0].g_dmat, dmam,
	    chain, flags);
	if (err)
		return (err);
	err = amdgart_reload(amdgart_softcs[0].g_ex, dmam);
	if (err)
		bus_dmamap_unload(amdgart_softcs[0].g_dmat, dmam);
	else
		amdgart_invalidate();
	return (err);
}

int
amdgart_dmamap_load_uio(bus_dma_tag_t tag, bus_dmamap_t dmam,
    struct uio *uio, int flags)
{
	int err;

	err = bus_dmamap_load_uio(amdgart_softcs[0].g_dmat, dmam, uio, flags);
	if (err)
		return (err);
	err = amdgart_reload(amdgart_softcs[0].g_ex, dmam);
	if (err)
		bus_dmamap_unload(amdgart_softcs[0].g_dmat, dmam);
	else
		amdgart_invalidate();
	return (err);
}

int
amdgart_dmamap_load_raw(bus_dma_tag_t tag, bus_dmamap_t dmam,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	int err;

	err = bus_dmamap_load_raw(amdgart_softcs[0].g_dmat, dmam, segs, nsegs,
	    size, flags);
	if (err)
		return (err);
	err = amdgart_reload(amdgart_softcs[0].g_ex, dmam);
	if (err)
		bus_dmamap_unload(amdgart_softcs[0].g_dmat, dmam);
	else
		amdgart_invalidate();
	return (err);
}

void
amdgart_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	int i;

	for (i = 0; i < dmam->dm_nsegs; i++)
		amdgart_iommu_unmap(amdgart_softcs[0].g_ex,
		    dmam->dm_segs[i].ds_addr, dmam->dm_segs[i].ds_len);
	/* XXX should we invalidate here? */
	bus_dmamap_unload(amdgart_softcs[0].g_dmat, dmam);
}

void
amdgart_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset,
    bus_size_t size, int ops)
{
	if (ops & (BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE)) {
		/*
		 * XXX this should be conditionalized... only do it
		 * XXX when necessary.
		 */
		amdgart_invalidate_wait();
	}

	/*
	 * XXX how do we deal with non-coherent mappings?  We don't
	 * XXX allow them right now.
	 */

	bus_dmamap_sync(amdgart_softcs[0].g_dmat, dmam, offset, size, ops);
}

int
amdgart_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	return (bus_dmamem_alloc(amdgart_softcs[0].g_dmat, size, alignment,
	    boundary, segs, nsegs, rsegs, flags));
}

void
amdgart_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	bus_dmamem_free(amdgart_softcs[0].g_dmat, segs, nsegs);
}

int
amdgart_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	return (bus_dmamem_map(amdgart_softcs[0].g_dmat, segs, nsegs, size,
	    kvap, flags));
}

void
amdgart_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva, size_t size)
{
	bus_dmamem_unmap(amdgart_softcs[0].g_dmat, kva, size);
}

paddr_t
amdgart_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    off_t off, int prot, int flags)
{
	return (bus_dmamem_mmap(amdgart_softcs[0].g_dmat, segs, nsegs, off,
	    prot, flags));
}
