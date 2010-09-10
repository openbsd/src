/*	$OpenBSD: iommu.c,v 1.30 2010/09/10 21:37:03 kettenis Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/lock.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

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

int amdgart_enable = 0;

#ifndef SMALL_KERNEL /* no bigmem in ramdisks */

struct amdgart_softc {
	pci_chipset_tag_t	 g_pc;
	paddr_t			 g_pa;
	paddr_t			 g_scribpa;
	void			*g_scrib;
	u_int32_t		 g_scribpte;
	u_int32_t		*g_pte;
	bus_dma_tag_t		 g_dmat;
	int			 g_count;
	pcitag_t		 g_tags[1];
};

void	amdgart_probe(struct pcibus_attach_args *);
void	amdgart_dumpregs(struct amdgart_softc *);
int	amdgart_ok(pci_chipset_tag_t, pcitag_t);
int	amdgart_enabled(pci_chipset_tag_t, pcitag_t);
void	amdgart_initpt(struct amdgart_softc *, u_long);
void	amdgart_bind_page(void *, bus_addr_t, paddr_t,  int);
void	amdgart_unbind_page(void *, bus_addr_t);
void	amdgart_invalidate(void *);
void	amdgart_invalidate_wait(struct amdgart_softc *);

struct bus_dma_tag amdgart_bus_dma_tag = {
	NULL,			/* _may_bounce */
	sg_dmamap_create,
	sg_dmamap_destroy,
	sg_dmamap_load,
	sg_dmamap_load_mbuf,
	sg_dmamap_load_uio,
	sg_dmamap_load_raw,
	sg_dmamap_unload,
	_bus_dmamap_sync,
	sg_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
amdgart_bind_page(void *handle, bus_addr_t offset, paddr_t page,  int flags)
{
	struct amdgart_softc	*sc = handle;
	u_int32_t		 pgno, pte;

	pgno = (offset - sc->g_pa) >> PGSHIFT;
	pte = GART_PTE_VALID | GART_PTE_COHERENT |
	    ((page >> 28) & GART_PTE_PHYSHI) | (page & GART_PTE_PHYSLO);
	sc->g_pte[pgno] = pte;
}

void
amdgart_unbind_page(void *handle, bus_addr_t offset)
{
	struct amdgart_softc	*sc = handle;
	u_int32_t		 pgno;

	pgno = (offset - sc->g_pa) >> PGSHIFT;
	sc->g_pte[pgno] = sc->g_scribpte;
}

void
amdgart_invalidate_wait(struct amdgart_softc *sc)
{
	int	i, n;

	for (n = 0; n < sc->g_count; n++) {
		for (i = 1000; i > 0; i--) {
			if ((pci_conf_read(sc->g_pc, sc->g_tags[n],
			    GART_CACHECTRL) & GART_CACHE_INVALIDATE) == 0)
				break;
			delay(1);
		}
		if (i == 0)
			printf("GART%d: timeout\n", n);
	}
}

void
amdgart_invalidate(void* handle)
{
	struct amdgart_softc	*sc = handle;
	int n;

	for (n = 0; n < sc->g_count; n++)
		pci_conf_write(sc->g_pc, sc->g_tags[n], GART_CACHECTRL,
		    GART_CACHE_INVALIDATE);
	amdgart_invalidate_wait(sc);
}

void
amdgart_dumpregs(struct amdgart_softc *sc)
{
	int n, i, dirty = 0;
	u_int8_t *p;

	for (n = 0; n < sc->g_count; n++) {
		printf("GART%d:\n", n);
		printf(" apctl %x\n", pci_conf_read(sc->g_pc, sc->g_tags[n],
		    GART_APCTRL));
		printf(" apbase %x\n", pci_conf_read(sc->g_pc, sc->g_tags[n],
		    GART_APBASE));
		printf(" tblbase %x\n", pci_conf_read(sc->g_pc, sc->g_tags[n],
		    GART_TBLBASE));
		printf(" cachectl %x\n", pci_conf_read(sc->g_pc, sc->g_tags[n],
		    GART_CACHECTRL));

	}
	p = sc->g_scrib;
	for (i = 0; i < PAGE_SIZE; i++, p++)
		if (*p != '\0')
			dirty++;
	printf(" scribble: %s\n", dirty ? "dirty" : "clean");
}

int
amdgart_ok(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t v;

	v = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(v) == PCI_VENDOR_AMD &&
	    (PCI_PRODUCT(v) == PCI_PRODUCT_AMD_AMD64_0F_MISC ||
	    PCI_PRODUCT(v) == PCI_PRODUCT_AMD_AMD64_10_MISC))
		return (1);
	return (0);
}

int
amdgart_enabled(pci_chipset_tag_t pc, pcitag_t tag)
{
	return (pci_conf_read(pc, tag, GART_APCTRL) & GART_APCTRL_ENABLE);
}

static const struct gart_size {
	pcireg_t	reg;
	bus_size_t	size;
} apsizes[] = {
	{ GART_APCTRL_SIZE_32M, 32 },
	{ GART_APCTRL_SIZE_64M, 64 },
	{ GART_APCTRL_SIZE_128M, 128 },
	{ GART_APCTRL_SIZE_256M, 256 },
	{ GART_APCTRL_SIZE_512M, 512 },
	{ GART_APCTRL_SIZE_1G, 1024 },
	{ GART_APCTRL_SIZE_2G, 2048 },
};

void
amdgart_probe(struct pcibus_attach_args *pba)
{
	struct amdgart_softc	*sc;
	struct sg_cookie	*cookie = NULL;
	void			*scrib = NULL;
	u_int32_t		*pte;
	int			 dev, count = 0, encount = 0, r, nseg;
	u_long			 mapsize, ptesize, gartsize = 0;
	bus_dma_segment_t	 seg;
	pcitag_t		 tag;
	pcireg_t		 v;
	paddr_t			 pa, ptepa;

	if (amdgart_enable == 0)
		return;

	/* Function is always three */
	for (count = 0, dev = 24; dev < 32; dev++) {
		tag = pci_make_tag(pba->pba_pc, 0, dev, 3);

		if (!amdgart_ok(pba->pba_pc, tag))
			continue;
		count++;
		if (amdgart_enabled(pba->pba_pc, tag)) {
			encount++;
			pa = pci_conf_read(pba->pba_pc, tag,
			    GART_APBASE) << 25;
			v = pci_conf_read(pba->pba_pc, tag,
			    GART_APCTRL) & GART_APCTRL_SIZE;
		}
	}

	if (count == 0)
		return;

	if (encount > 0 && encount != count) {
		printf("\niommu: holy mismatched enabling, batman!\n");
		return;
	}

	sc = malloc(sizeof(*sc) + (sizeof(pcitag_t) * (count - 1)),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		printf("\nGART: can't get softc");
		return;
	}

	if (encount > 0) {
		int i;
		/*
		 * GART exists, use the current value.
		 *
		 * It appears that the bios mentions this in it's memory map
		 * (sample size of 1), so we don't need to allocate
		 * address space for it.
		 */
		sc->g_pa = pa;
		for (i = 0; i < nitems(apsizes); i++)
			if (apsizes[i].reg == v)
				gartsize = apsizes[i].size;
		if (gartsize == 0) {
			printf("iommu: strange size\n");
			free(sc, M_DEVBUF);
			return;
		}

		mapsize = gartsize * 1024 * 1024;
	} else {
		/* We've gotta allocate one. Heuristic time! */
		/*
		 * XXX right now we stuff the iommu where we want. this need
		 * XXX changing to allocate from pci space.
		 */
		sc->g_pa = IOMMU_START;
		gartsize = IOMMU_SIZE;
	}

	mapsize = gartsize * 1024 * 1024;
	ptesize = mapsize / (PAGE_SIZE / sizeof(u_int32_t));

	/*
	 * use the low level version so we know we get memory we can use.
	 * Hardware can deal with up to 40bits, which should be all our memory,
	 * be safe anyway.
	 */
	r = _bus_dmamem_alloc_range(pba->pba_dmat, ptesize, ptesize, ptesize,
	    &seg, 1, &nseg, BUS_DMA_NOWAIT, ptesize, 0x4000000000);
	if (r != 0) {
		printf("\nGART: failed to get pte pages");
		goto nofreeseg;
	}

	r = _bus_dmamem_map(pba->pba_dmat, &seg, 1, ptesize, (caddr_t *)&pte,
	    BUS_DMA_NOWAIT | BUS_DMA_NOCACHE);
	if (r != 0) {
		printf("\nGART: failed to map pte pages");
		goto err;
	}
	ptepa = seg.ds_addr;

	scrib = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (scrib == NULL) {
		printf("\nGART: didn't get scribble page");
		goto err;
	}
	sc->g_pc = pba->pba_pc;
	pmap_extract(pmap_kernel(), (vaddr_t)scrib,
	    &sc->g_scribpa);
	sc->g_scrib = scrib;
	sc->g_scribpte = GART_PTE_VALID | GART_PTE_COHERENT |
	    ((sc->g_scribpa >> 28) & GART_PTE_PHYSHI) |
	    (sc->g_scribpa & GART_PTE_PHYSLO);
	sc->g_pte = pte;
	sc->g_dmat = pba->pba_dmat;

	if ((cookie = sg_dmatag_init("iommu", sc, sc->g_pa, mapsize,
	    amdgart_bind_page, amdgart_unbind_page,
	    amdgart_invalidate)) == NULL) {
		printf("\nGART: didn't get dma cookie\n");
		goto err;
	}

	for (count = 0, dev = 24; dev < 32; dev++) {
		tag = pci_make_tag(pba->pba_pc, 0, dev, 3);

		if (!amdgart_ok(pba->pba_pc, tag))
			continue;

		v = pci_conf_read(pba->pba_pc, tag, GART_APCTRL);
		v |= GART_APCTRL_DISCPU | GART_APCTRL_DISTBL |
		    GART_APCTRL_DISIO;
		v &= ~(GART_APCTRL_ENABLE | GART_APCTRL_SIZE);
		switch (gartsize) {
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
		    sc->g_pa >> 25);

		pci_conf_write(pba->pba_pc, tag, GART_TBLBASE,
		    (ptepa >> 8) & GART_TBLBASE_MASK);

		v = pci_conf_read(pba->pba_pc, tag, GART_APCTRL);
		v |= GART_APCTRL_ENABLE;
		v &= ~GART_APCTRL_DISIO;
		pci_conf_write(pba->pba_pc, tag, GART_APCTRL, v);

		sc->g_tags[count] = tag;

		printf("\niommu%d at cpu%d: base 0x%lx length %dMB"
		    " pte 0x%lx", count, dev - 24, sc->g_pa,
		    gartsize, ptepa);
		count++;
	}
	amdgart_initpt(sc, ptesize / sizeof(*sc->g_pte));
	sc->g_count = count;

	amdgart_bus_dma_tag._cookie = cookie;
	pba->pba_dmat = &amdgart_bus_dma_tag;

	return;

err:
	_bus_dmamem_free(pba->pba_dmat, &seg, 1);
nofreeseg:
	if (scrib != NULL)
		free(scrib, M_DEVBUF);
	if (cookie != NULL)
		sg_dmatag_destroy(cookie);
	if (sc != NULL)
		free(sc, M_DEVBUF);
}

void
amdgart_initpt(struct amdgart_softc *sc, u_long nent)
{
	u_long i;

	for (i = 0; i < nent; i++)
		sc->g_pte[i] = sc->g_scribpte;
	amdgart_invalidate(sc);
}

#endif /* !SMALL_KERNEL */
