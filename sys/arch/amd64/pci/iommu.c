/*	$OpenBSD: iommu.c,v 1.2 2005/05/26 19:47:44 jason Exp $	*/

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
 *	- scribble page for devices that lie about being done with memory.
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

u_int32_t *gartpt;
struct pglist gartplist;
struct extent *gartex;
pci_chipset_tag_t gartpc;
pcitag_t garttag;
bus_dma_tag_t gartparent;
paddr_t gartpa;
paddr_t gartscribpa;
void *gartscrib;
u_int32_t gartscribflags;

void amdgart_invalidate_wait(pci_chipset_tag_t, pcitag_t);
void amdgart_invalidate(pci_chipset_tag_t, pcitag_t);
void amdgart_probe(struct pcibus_attach_args *);
int amdgart_initpte(pci_chipset_tag_t, pcitag_t, paddr_t, psize_t, psize_t);
void amdgart_dumpregs(void);
int amdgart_iommu_map(struct extent *, paddr_t, paddr_t *, psize_t);
int amdgart_iommu_unmap(struct extent *, paddr_t, psize_t);
int amdgart_reload(struct extent *, bus_dmamap_t);

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
amdgart_invalidate_wait(pci_chipset_tag_t pc, pcitag_t tag)
{
	u_int32_t v;
	int i;

	for (i = 1000; i > 0; i--) {
		v = pci_conf_read(NULL, tag, GART_CACHECTRL);
		if ((v & GART_CACHE_INVALIDATE) == 0)
			break;
		delay(1);
	}
	if (i == 0)
		printf("GART: invalidate timeout\n");
}

void
amdgart_invalidate(pci_chipset_tag_t pc, pcitag_t tag)
{
	pci_conf_write(pc, tag, GART_CACHECTRL, GART_CACHE_INVALIDATE);
}

int
amdgart_initpte(pci_chipset_tag_t pc, pcitag_t tag, paddr_t base,
    psize_t mapsize, psize_t sz)
{
	struct vm_page *m;
	vaddr_t va;
	paddr_t pa, off;
	u_int32_t r, *pte;
	int err;

	TAILQ_INIT(&gartplist);

	gartscrib = (void *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (gartscrib == NULL) {
		printf("\nGART: failed to get scribble page");
		goto err;
	}
	pmap_extract(pmap_kernel(), (vaddr_t)gartscrib, &gartscribpa);
	gartscribflags = GART_PTE_VALID | GART_PTE_COHERENT |
	    ((gartscribpa >> 28) & GART_PTE_PHYSHI) |
	     (gartscribpa & GART_PTE_PHYSLO);

	err = uvm_pglistalloc(sz, sz, trunc_page(avail_end), sz, sz,
	    &gartplist, 1, 0);
	if (err) {
		printf("\nGART: failed to get PTE pages: %d", err);
		goto err;
	}
	va = uvm_km_valloc(kernel_map, sz);
	if (va == 0) {
		printf("\nGART: failed to get PTE vspace");
		goto err;
	}
	gartpt = (u_int32_t *)va;

	gartex = extent_create("iommu", base, base + mapsize - 1, M_DEVBUF,
	    NULL, NULL, EX_NOWAIT | EX_NOCOALESCE);
	gartpa = base;
	if (gartex == NULL) {
		printf("\nGART: can't create extent");
		goto err;
	}
	printf("\n");
	extent_print(gartex);

	m = TAILQ_FIRST(&gartplist);
	pa = VM_PAGE_TO_PHYS(m);
	for (off = 0; m; m = TAILQ_NEXT(m, pageq), off += PAGE_SIZE) {
		if (VM_PAGE_TO_PHYS(m) != (pa + off)) {
			printf("\nGART: too many segments!");
			goto err;
		}
		/* XXX check for error?  art? */
		pmap_enter(pmap_kernel(), va + off, pa + off,
		    VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
	}
	pmap_update(pmap_kernel());

	pci_conf_write(NULL, tag, GART_TBLBASE, (pa >> 8) & GART_TBLBASE_MASK);

	for (r = 0, pte = gartpt; r < (sz / sizeof(*gartpt)); r++, pte++)
		*pte = gartscribflags;
	amdgart_invalidate(pc, tag);
	amdgart_invalidate_wait(pc, tag);
		
	return (0);

err:
	if (gartscrib)
		free(gartscrib, M_DEVBUF);
	if (!TAILQ_EMPTY(&gartplist))
		uvm_pglistfree(&gartplist);
	if (gartex != NULL) {
		extent_destroy(gartex);
		gartex = NULL;
	}
	return (-1);
}

void
amdgart_dumpregs(void)
{
	printf("apctl %x\n", pci_conf_read(gartpc, garttag, GART_APCTRL));
	printf("apbase %x\n", pci_conf_read(gartpc, garttag, GART_APBASE));
	printf("tblbase %x\n", pci_conf_read(gartpc, garttag, GART_TBLBASE));
	printf("cachectl %x\n", pci_conf_read(gartpc, garttag, GART_CACHECTRL));
}

void
amdgart_probe(struct pcibus_attach_args *pba)
{
	pci_chipset_tag_t pc = pba->pba_pc;
	pcitag_t tag;
	u_int32_t apctl, v;
	u_long base;
	int r;

	gartpc = pc;
	garttag = tag = pci_make_tag(pc, 0, 24, 3);
	gartparent = pba->pba_dmat;
	v = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(v) != PCI_VENDOR_AMD ||
	    PCI_PRODUCT(v) != PCI_PRODUCT_AMD_AMD64_MISC) {
		printf("\ndidn't find misc registers, no gart.");
		return;
	}

	apctl = pci_conf_read(pc, tag, GART_APCTRL);
	if (apctl & GART_APCTRL_ENABLE) {
		printf("\nBIOS already enabled it, this is hard, no gart.");
		return;
	}

	r = extent_alloc_subregion(iomem_ex, IOMMU_START, IOMMU_END,
	    IOMMU_SIZE * 1024 * 1024, IOMMU_ALIGN * 1024 * 1024, 0,
	    EX_NOBOUNDARY, EX_NOWAIT, &base);
	if (r != 0) {
		printf("\nGART extent alloc failed: %d", r);
		return;
	}

	apctl &= ~GART_APCTRL_SIZE;
	switch (IOMMU_SIZE) {
	case 32:
		apctl |= GART_APCTRL_SIZE_32M;
		break;
	case 64:
		apctl |= GART_APCTRL_SIZE_64M;
		break;
	case 128:
		apctl |= GART_APCTRL_SIZE_128M;
		break;
	case 256:
		apctl |= GART_APCTRL_SIZE_256M;
		break;
	case 512:
		apctl |= GART_APCTRL_SIZE_512M;
		break;
	case 1024:
		apctl |= GART_APCTRL_SIZE_1G;
		break;
	case 2048:
		apctl |= GART_APCTRL_SIZE_2G;
		break;
	default:
		printf("\nGART: bad size");
		return;
	}
	apctl |= GART_APCTRL_ENABLE | GART_APCTRL_DISCPU | GART_APCTRL_DISTBL |
	    GART_APCTRL_DISIO;
	pci_conf_write(pc, tag, GART_APCTRL, apctl);
	pci_conf_write(pc, tag, GART_APBASE, base >> 25);

	v = ((IOMMU_SIZE * 1024) / (PAGE_SIZE / sizeof(u_int32_t))) * 1024;
	if (amdgart_initpte(pc, tag, base, IOMMU_SIZE * 1024 * 1024, v)) {
		printf("\nGART: initpte failed");
		return;
	}
	apctl &= ~(GART_APCTRL_DISIO | GART_APCTRL_DISTBL);
	pci_conf_write(pc, tag, GART_APCTRL, apctl);
	printf("\nGART base 0x%08x (%uMB) pte 0x%lx",
	    base, v / 1024, VM_PAGE_TO_PHYS(TAILQ_FIRST(&gartplist)));

	/* switch to our own bus_dma_tag_t */
	pba->pba_dmat = &amdgart_bus_dma_tag;
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
		err = amdgart_iommu_map(gartex, opa, &npa, len);
		if (err) {
			for (j = 0; j < i - 1; j++)
				amdgart_iommu_unmap(ex, opa, len);
			return (err);
		}
		dmam->dm_segs[i].ds_addr = npa;
	}
	return (0);
}

int
amdgart_iommu_map(struct extent *ex, paddr_t opa, paddr_t *npa, psize_t len)
{
	paddr_t base, end, idx;
	psize_t alen;
	u_long res;
	int err;
	u_int32_t pgno, flags;

	base = trunc_page(opa);
	end = roundup(opa + len, PAGE_SIZE);
	alen = end - base;
	err = extent_alloc(ex, alen, PAGE_SIZE, 0, EX_NOBOUNDARY,
	    EX_NOWAIT, &res);
	if (err) {
		printf("GART: extent_alloc %d\n", err);
		return (err);
	}
	*npa = res | (opa & PGOFSET);

	for (idx = 0; idx < alen; idx += PAGE_SIZE) {
		pgno = ((res + idx) - gartpa) >> PGSHIFT;
		flags = GART_PTE_VALID | GART_PTE_COHERENT |
		    (((base + idx) >> 28) & GART_PTE_PHYSHI) |
		     ((base + idx) & GART_PTE_PHYSLO);
		gartpt[pgno] = flags;
	}

	return (0);
}

int
amdgart_iommu_unmap(struct extent *ex, paddr_t pa, psize_t len)
{
	paddr_t base, end, idx;
	psize_t alen;
	int err;
	u_int32_t pgno;

	base = trunc_page(pa);
	end = roundup(pa + len, PAGE_SIZE);
	alen = end - base;
	err = extent_free(ex, base, alen, EX_NOWAIT);
	if (err) {
		printf("GART: extent_free %d\n", err);
		return (err);
	}

	for (idx = 0; idx < alen; idx += PAGE_SIZE) {
		pgno = ((base - gartpa) + idx) >> PGSHIFT;
		gartpt[pgno] = gartscribflags;
	}

	return (0);
}

int
amdgart_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	static int once = 0;

	if (!once) {
		once = 1;
		printf("using AMDGART bus_dma!\n");
	}
	return (bus_dmamap_create(gartparent, size, nsegments, maxsegsz,
	    boundary, flags, dmamp));
}

void
amdgart_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	bus_dmamap_destroy(gartparent, dmam);
}

int
amdgart_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	int err;

	err = bus_dmamap_load(gartparent, dmam, buf, buflen, p, flags);
	if (err)
		return (err);
	err = amdgart_reload(gartex, dmam);
	if (err)
		bus_dmamap_unload(gartparent, dmam);
	else
		amdgart_invalidate(gartpc, garttag);
	return (err);
}

int
amdgart_dmamap_load_mbuf(bus_dma_tag_t tag, bus_dmamap_t dmam,
    struct mbuf *chain, int flags)
{
	int err;

	err = bus_dmamap_load_mbuf(gartparent, dmam, chain, flags);
	if (err)
		return (err);
	err = amdgart_reload(gartex, dmam);
	if (err)
		bus_dmamap_unload(gartparent, dmam);
	else
		amdgart_invalidate(gartpc, garttag);
	return (err);
}

int
amdgart_dmamap_load_uio(bus_dma_tag_t tag, bus_dmamap_t dmam,
    struct uio *uio, int flags)
{
	int err;

	err = bus_dmamap_load_uio(gartparent, dmam, uio, flags);
	if (err)
		return (err);
	err = amdgart_reload(gartex, dmam);
	if (err)
		bus_dmamap_unload(gartparent, dmam);
	else
		amdgart_invalidate(gartpc, garttag);
	return (err);
}

int
amdgart_dmamap_load_raw(bus_dma_tag_t tag, bus_dmamap_t dmam,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	int err;

	err = bus_dmamap_load_raw(gartparent, dmam, segs, nsegs, size, flags);
	if (err)
		return (err);
	err = amdgart_reload(gartex, dmam);
	if (err)
		bus_dmamap_unload(gartparent, dmam);
	else
		amdgart_invalidate(gartpc, garttag);
	return (err);
}

void
amdgart_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	int i;

	for (i = 0; i < dmam->dm_nsegs; i++)
		amdgart_iommu_unmap(gartex, dmam->dm_segs[i].ds_addr,
		    dmam->dm_segs[i].ds_len);
	/* XXX should we invalidate here? */
	bus_dmamap_unload(gartparent, dmam);
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
		amdgart_invalidate_wait(gartpc, garttag);
	}

	/*
	 * XXX how do we deal with non-coherent mappings?  We don't
	 * XXX allow them right now.
	 */

	bus_dmamap_sync(gartparent, dmam, offset, size, ops);
}

int
amdgart_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	return (bus_dmamem_alloc(gartparent, size, alignment, boundary, segs,
	    nsegs, rsegs, flags));
}

void
amdgart_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	bus_dmamem_free(gartparent, segs, nsegs);
}

int
amdgart_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	return (bus_dmamem_map(gartparent, segs, nsegs, size, kvap, flags));
}

void
amdgart_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva, size_t size)
{
	bus_dmamem_unmap(gartparent, kva, size);
}

paddr_t
amdgart_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    off_t off, int prot, int flags)
{
	return (bus_dmamem_mmap(gartparent, segs, nsegs, off, prot, flags));
}
