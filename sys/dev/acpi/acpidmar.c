/*
 * Copyright (c) 2015 Jordan Hargrave <jordan_hargrave@hotmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/apicvar.h>
#include <machine/biosvar.h>
#include <machine/cpuvar.h>
#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <uvm/uvm_extern.h>

#include <machine/i8259.h>
#include <machine/i82093reg.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>

#include <machine/mpbiosvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include "ioapic.h"

#include "acpidmar.h"

#define dprintf(x...)

#ifdef DDB
int	acpidmar_ddb = 0;
#endif

int	intel_iommu_gfx_mapped = 0;
int	force_cm = 1;

void showahci(void *);

/* Page Table Entry per domain */
struct iommu_softc;

static inline int
mksid(int b, int d, int f)
{
	return (b << 8) + (d << 3) + f;
}

static inline int
sid_devfn(int sid)
{
	return sid & 0xff;
}

static inline int
sid_bus(int sid)
{
	return (sid >> 8) & 0xff;
}

static inline int
sid_dev(int sid)
{
	return (sid >> 3) & 0x1f;
}

static inline int
sid_fun(int sid)
{
	return (sid >> 0) & 0x7;
}

struct domain_dev {
	int			sid;
	TAILQ_ENTRY(domain_dev)	link;
};

struct domain {
	struct iommu_softc	*iommu;
	int			did;
	int			gaw;
	struct pte_entry	*pte;
	paddr_t			ptep;
	struct bus_dma_tag	dmat;
	int			flag;

	struct mutex            exlck;
	char			exname[32];
	struct extent		*iovamap;
	TAILQ_HEAD(,domain_dev)	devices;
	TAILQ_ENTRY(domain)	link;
};

#define DOM_DEBUG 0x1
#define DOM_NOMAP 0x2

struct dmar_devlist {
	int				type;
	int				bus;
	int				ndp;
	struct acpidmar_devpath		*dp;
	TAILQ_ENTRY(dmar_devlist)	link;
};

TAILQ_HEAD(devlist_head, dmar_devlist);

struct rmrr_softc {
	TAILQ_ENTRY(rmrr_softc)	link;
	struct devlist_head	devices;
	int			segment;
	uint64_t		start;
	uint64_t		end;
};

struct atsr_softc {
	TAILQ_ENTRY(atsr_softc)	link;
	struct devlist_head	devices;
	int			segment;
	int			flags;
};

struct iommu_pic {
	struct pic		pic;
	struct iommu_softc	*iommu;
};

#define IOMMU_FLAGS_CATCHALL		0x1
#define IOMMU_FLAGS_BAD			0x2
#define IOMMU_FLAGS_SUSPEND		0x4

struct iommu_softc {
	TAILQ_ENTRY(iommu_softc)link;
	struct devlist_head	devices;
	int			id;
	int			flags;
	int			segment;

	struct mutex		reg_lock;

	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;

	uint64_t		cap;
	uint64_t		ecap;
	uint32_t		gcmd;

	int			mgaw;
	int			agaw;
	int			ndoms;

	struct root_entry	*root;
	struct context_entry	*ctx[256];

	void			*intr;
	struct iommu_pic	pic;
	int			fedata;
	uint64_t		feaddr;
	uint64_t		rtaddr;

	// Queued Invalidation
	int			qi_head;
	int			qi_tail;
	paddr_t			qip;
	struct qi_entry		*qi;

	struct domain		*unity;
	TAILQ_HEAD(,domain)	domains;
};

static inline int iommu_bad(struct iommu_softc *sc)
{
	return (sc->flags & IOMMU_FLAGS_BAD);
}

static inline int iommu_enabled(struct iommu_softc *sc)
{
	return (sc->gcmd & GCMD_TE);
}

struct acpidmar_softc {
	struct device		sc_dev;

	pci_chipset_tag_t	sc_pc;
	bus_space_tag_t		sc_memt;
	int			sc_haw;
	int			sc_flags;

	TAILQ_HEAD(,iommu_softc)sc_drhds;
	TAILQ_HEAD(,rmrr_softc)	sc_rmrrs;
	TAILQ_HEAD(,atsr_softc)	sc_atsrs;
};

int		acpidmar_activate(struct device *, int);
int		acpidmar_match(struct device *, void *, void *);
void		acpidmar_attach(struct device *, struct device *, void *);
struct domain   *acpidmar_pci_attach(struct acpidmar_softc *, int, int, int);

struct cfattach acpidmar_ca = {
	sizeof(struct acpidmar_softc), acpidmar_match, acpidmar_attach,
};

struct cfdriver acpidmar_cd = {
	NULL, "acpidmar", DV_DULL
};

struct		acpidmar_softc *acpidmar_sc;
int		acpidmar_intr(void *);

#define DID_UNITY 0x1

struct domain *domain_create(struct iommu_softc *, int);
struct domain *domain_lookup(struct acpidmar_softc *, int, int);

void domain_unload_map(struct domain *, bus_dmamap_t);
void domain_load_map(struct domain *, bus_dmamap_t, int, int, const char *);

void domain_map_page(struct domain *, vaddr_t, paddr_t, int);
void domain_map_pthru(struct domain *, paddr_t, paddr_t);

void acpidmar_pci_hook(pci_chipset_tag_t, struct pci_attach_args *);
void acpidmar_parse_devscope(union acpidmar_entry *, int, int,
    struct devlist_head *);
int  acpidmar_match_devscope(struct devlist_head *, pci_chipset_tag_t, int);

void acpidmar_init(struct acpidmar_softc *, struct acpi_dmar *);
void acpidmar_drhd(struct acpidmar_softc *, union acpidmar_entry *);
void acpidmar_rmrr(struct acpidmar_softc *, union acpidmar_entry *);
void acpidmar_atsr(struct acpidmar_softc *, union acpidmar_entry *);

void *acpidmar_intr_establish(void *, int, int (*)(void *), void *,
    const char *);

void iommu_writel(struct iommu_softc *, int, uint32_t);
uint32_t iommu_readl(struct iommu_softc *, int);
void iommu_writeq(struct iommu_softc *, int, uint64_t);
uint64_t iommu_readq(struct iommu_softc *, int);
void iommu_showfault(struct iommu_softc *, int,
    struct fault_entry *);
void iommu_showcfg(struct iommu_softc *, int);

int iommu_init(struct acpidmar_softc *, struct iommu_softc *,
    struct acpidmar_drhd *);
int iommu_enable_translation(struct iommu_softc *, int);
void iommu_enable_qi(struct iommu_softc *, int);
void iommu_flush_cache(struct iommu_softc *, void *, size_t);
void *iommu_alloc_page(struct iommu_softc *, paddr_t *);
void iommu_flush_write_buffer(struct iommu_softc *);
void iommu_issue_qi(struct iommu_softc *, struct qi_entry *);

void iommu_flush_ctx(struct iommu_softc *, int, int, int, int);
void iommu_flush_ctx_qi(struct iommu_softc *, int, int, int, int);
void iommu_flush_tlb(struct iommu_softc *, int, int);
void iommu_flush_tlb_qi(struct iommu_softc *, int, int);

void iommu_set_rtaddr(struct iommu_softc *, paddr_t);
void acpidmar_sw(int);

const char *dmar_bdf(int);

const char *
dmar_bdf(int sid)
{
	static char	bdf[32];

	snprintf(bdf, sizeof(bdf), "%.4x:%.2x:%.2x.%x", 0,
	    sid_bus(sid), sid_dev(sid), sid_fun(sid));

	return (bdf);
}

/* busdma */
static int dmar_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
    bus_size_t, int, bus_dmamap_t *);
static void dmar_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
static int dmar_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    struct proc *, int);
static int dmar_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *,
    int);
static int dmar_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t, struct uio *, int);
static int dmar_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
    bus_dma_segment_t *, int, bus_size_t, int);
static void dmar_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
static void dmar_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    bus_size_t, int);
static int dmar_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
static void dmar_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
static int dmar_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
static void dmar_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
static paddr_t	dmar_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int, off_t,
    int, int);

static void dmar_dumpseg(bus_dma_tag_t, int, bus_dma_segment_t *, const char *);
const char *dom_bdf(struct domain *dom);
void domain_map_check(struct domain *dom);

static inline int
debugme(struct domain *dom)
{
	return (dom->flag & DOM_DEBUG);
}

void
domain_map_check(struct domain *dom)
{
	struct iommu_softc *iommu;
	struct domain_dev *dd;
	struct context_entry *ctx;
	int v;

	iommu = dom->iommu;
	TAILQ_FOREACH(dd, &dom->devices, link) {
		acpidmar_pci_attach(acpidmar_sc, iommu->segment, dd->sid, 1);

		/* Check if this is the first time we are mapped */
		ctx = &iommu->ctx[sid_bus(dd->sid)][sid_devfn(dd->sid)];
		v = context_user(ctx);
		if (v != 0xA) {
			printf("  map: %.4x:%.2x:%.2x.%x iommu:%d did:%.4x\n",
			    iommu->segment,
			    sid_bus(dd->sid),
			    sid_dev(dd->sid),
			    sid_fun(dd->sid),
			    iommu->id,
			    dom->did);
			context_set_user(ctx, 0xA);
		}
	}
}

/* Map a single page as passthrough - used for DRM */
void
dmar_ptmap(bus_dma_tag_t tag, bus_addr_t addr)
{
	struct domain *dom = tag->_cookie;

	if (!acpidmar_sc)
		return;
	domain_map_check(dom);
	domain_map_page(dom, addr, addr, PTE_P | PTE_R | PTE_W);
}

/* Map a range of pages 1:1 */
void
domain_map_pthru(struct domain *dom, paddr_t start, paddr_t end)
{
	domain_map_check(dom);
	while (start < end) {
		domain_map_page(dom, start, start, PTE_P | PTE_R | PTE_W);
		start += VTD_PAGE_SIZE;
	}
}

/* Map a single paddr to IOMMU paddr */
void
domain_map_page(struct domain *dom, vaddr_t va, paddr_t pa, int flags)
{
	paddr_t paddr;
	struct pte_entry *pte, *npte;
	int lvl, idx;
	struct iommu_softc *iommu;

	iommu = dom->iommu;
	/* Insert physical address into virtual address map
	 * XXX: could we use private pmap here?
	 * essentially doing a pmap_enter(map, va, pa, prot);
	 */

	/* Only handle 4k pages for now */
	npte = dom->pte;
	for (lvl = iommu->agaw - VTD_STRIDE_SIZE; lvl>= VTD_LEVEL0;
	    lvl -= VTD_STRIDE_SIZE) {
		idx = (va >> lvl) & VTD_STRIDE_MASK;
		pte = &npte[idx];
		if (lvl == VTD_LEVEL0) {
			/* Level 1: Page Table - add physical address */
			pte->val = pa | flags;
			iommu_flush_cache(iommu, pte, sizeof(*pte));
			break;
		} else if (!(pte->val & PTE_P)) {
			/* Level N: Point to lower level table */
			iommu_alloc_page(iommu, &paddr);
			pte->val = paddr | PTE_P | PTE_R | PTE_W;
			iommu_flush_cache(iommu, pte, sizeof(*pte));
		}
		npte = (void *)PMAP_DIRECT_MAP((pte->val & ~VTD_PAGE_MASK));
	}
}

static void
dmar_dumpseg(bus_dma_tag_t tag, int nseg, bus_dma_segment_t *segs,
    const char *lbl)
{
	struct domain	*dom = tag->_cookie;
	int i;

	return;
	if (!debugme(dom))
		return;
	printf("%s: %s\n", lbl, dom_bdf(dom));
	for (i = 0; i < nseg; i++) {
		printf("  %.16llx %.8x\n",
		    (uint64_t)segs[i].ds_addr,
		    (uint32_t)segs[i].ds_len);
	}
}

/* Unload mapping */
void
domain_unload_map(struct domain *dom, bus_dmamap_t dmam)
{
	bus_dma_segment_t	*seg;
	paddr_t			base, end, idx;
	psize_t			alen;
	int			i;

	if (iommu_bad(dom->iommu)) {
		printf("unload map no iommu\n");
		return;
	}

	acpidmar_intr(dom->iommu);
	for (i = 0; i < dmam->dm_nsegs; i++) {
		seg  = &dmam->dm_segs[i];

		base = trunc_page(seg->ds_addr);
		end  = roundup(seg->ds_addr + seg->ds_len, VTD_PAGE_SIZE);
		alen = end - base;

		if (debugme(dom)) {
			printf("  va:%.16llx len:%x\n",
			    (uint64_t)base, (uint32_t)alen);
		}

		/* Clear PTE */
		for (idx = 0; idx < alen; idx += VTD_PAGE_SIZE)
			domain_map_page(dom, base + idx, 0, 0);

		if (dom->flag & DOM_NOMAP) {
			printf("%s: nomap %.16llx\n", dom_bdf(dom), (uint64_t)base);
			continue;
		}		

		mtx_enter(&dom->exlck);
		if (extent_free(dom->iovamap, base, alen, EX_NOWAIT)) {
			panic("domain_unload_map: extent_free");
		}
		mtx_leave(&dom->exlck);
	}
}

/* map.segs[x].ds_addr is modified to IOMMU virtual PA */
void
domain_load_map(struct domain *dom, bus_dmamap_t map, int flags, int pteflag, const char *fn)
{
	bus_dma_segment_t	*seg;
	struct iommu_softc	*iommu;
	paddr_t			base, end, idx;
	psize_t			alen;
	u_long			res;
	int			i;

	iommu = dom->iommu;
	if (!iommu_enabled(iommu)) {
		/* Lazy enable translation when required */
		if (iommu_enable_translation(iommu, 1)) {
			return;
		}
	}
	domain_map_check(dom);
	acpidmar_intr(iommu);
	for (i = 0; i < map->dm_nsegs; i++) {
		seg = &map->dm_segs[i];

		base = trunc_page(seg->ds_addr);
		end  = roundup(seg->ds_addr + seg->ds_len, VTD_PAGE_SIZE);
		alen = end - base;
		res  = base;

		if (dom->flag & DOM_NOMAP) {
			goto nomap;
		}

		/* Allocate DMA Virtual Address */
		mtx_enter(&dom->exlck);
		if (extent_alloc(dom->iovamap, alen, VTD_PAGE_SIZE, 0,
		    map->_dm_boundary, EX_NOWAIT, &res)) {
			panic("domain_load_map: extent_alloc");
		}
		if (res == -1) {
			panic("got -1 address\n");
		}
		mtx_leave(&dom->exlck);

		if (debugme(dom)) {
			printf("  %.16llx %x => %.16llx\n",
			    (uint64_t)seg->ds_addr, (uint32_t)seg->ds_len,
			    (uint64_t)res);
		}

		/* Reassign DMA address */
		seg->ds_addr = res | (seg->ds_addr & VTD_PAGE_MASK);
nomap:
		for (idx = 0; idx < alen; idx += VTD_PAGE_SIZE) {
			domain_map_page(dom, res + idx, base + idx,
			    PTE_P | pteflag);
		}
	}
	if ((iommu->cap & CAP_CM) || force_cm) {
		iommu_flush_tlb(iommu, IOTLB_DOMAIN, dom->did);
	} else {
		iommu_flush_write_buffer(iommu);
	}
}

/* Bus DMA Map functions */
const char *
dom_bdf(struct domain *dom)
{
	struct domain_dev	*dd;
	static char		mmm[48];

	dd = TAILQ_FIRST(&dom->devices);
	snprintf(mmm, sizeof(mmm), "%s iommu:%d did:%.4x%s",
	    dmar_bdf(dd->sid), dom->iommu->id, dom->did,
	    dom->did == DID_UNITY ? " [unity]" : "");

	return (mmm);
}

static int
dmar_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	int rc;

	rc = _bus_dmamap_create(tag, size, nsegments, maxsegsz, boundary,
	    flags, dmamp);
	if (!rc) {
		dmar_dumpseg(tag, (*dmamp)->dm_nsegs, (*dmamp)->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static void
dmar_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	_bus_dmamap_destroy(tag, dmam);
}

static int
dmar_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct domain	*dom = tag->_cookie;
	int		rc;

	rc = _bus_dmamap_load(tag, dmam, buf, buflen, p, flags);
	if (!rc) {
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		domain_load_map(dom, dmam, flags, PTE_R|PTE_W, __FUNCTION__);
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static int
dmar_dmamap_load_mbuf(bus_dma_tag_t tag, bus_dmamap_t dmam, struct mbuf *chain,
    int flags)
{
	struct domain	*dom = tag->_cookie;
	int		rc;

	rc = _bus_dmamap_load_mbuf(tag, dmam, chain, flags);
	if (!rc) {
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		domain_load_map(dom, dmam, flags, PTE_R|PTE_W,__FUNCTION__);
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static int
dmar_dmamap_load_uio(bus_dma_tag_t tag, bus_dmamap_t dmam, struct uio *uio,
    int flags)
{
	struct domain	*dom = tag->_cookie;
	int		rc;

	rc = _bus_dmamap_load_uio(tag, dmam, uio, flags);
	if (!rc) {
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		domain_load_map(dom, dmam, flags, PTE_R|PTE_W, __FUNCTION__);
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static int
dmar_dmamap_load_raw(bus_dma_tag_t tag, bus_dmamap_t dmam,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	struct domain *dom = tag->_cookie;
	int rc;

	rc = _bus_dmamap_load_raw(tag, dmam, segs, nsegs, size, flags);
	if (!rc) {
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		domain_load_map(dom, dmam, flags, PTE_R|PTE_W, __FUNCTION__);
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static void
dmar_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	struct domain	*dom = tag->_cookie;

	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	domain_unload_map(dom, dmam);
	_bus_dmamap_unload(tag, dmam);
}

static void
dmar_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset,
    bus_size_t len, int ops)
{
	struct domain	*dom = tag->_cookie;
	//int		flag;

	//flag = PTE_P;
	acpidmar_intr(dom->iommu);
	//if (ops == BUS_DMASYNC_PREREAD) {
	//	/* make readable */
	//	flag |= PTE_R;
	//}
	//if (ops == BUS_DMASYNC_PREWRITE) {
	//	/* make writeable */
	//	flag |= PTE_W;
	//}
	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	_bus_dmamap_sync(tag, dmam, offset, len, ops);
}

static int
dmar_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	int rc;

	rc = _bus_dmamem_alloc(tag, size, alignment, boundary, segs, nsegs,
	    rsegs, flags);
	if (!rc) {
		dmar_dumpseg(tag, *rsegs, segs, __FUNCTION__);
	}
	return (rc);
}

static void
dmar_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	dmar_dumpseg(tag, nsegs, segs, __FUNCTION__);
	_bus_dmamem_free(tag, segs, nsegs);
}

static int
dmar_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	dmar_dumpseg(tag, nsegs, segs, __FUNCTION__);
	return (_bus_dmamem_map(tag, segs, nsegs, size, kvap, flags));
}

static void
dmar_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva, size_t size)
{
	struct domain	*dom = tag->_cookie;

	if (debugme(dom)) {
		printf("dmamap_unmap: %s\n", dom_bdf(dom));
	}
	_bus_dmamem_unmap(tag, kva, size);
}

static paddr_t
dmar_dmamem_mmap(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    off_t off, int prot, int flags)
{
	dmar_dumpseg(tag, nsegs, segs, __FUNCTION__);
	return (_bus_dmamem_mmap(tag, segs, nsegs, off, prot, flags));
}

/*===================================
 * IOMMU code
 *===================================*/
void
iommu_set_rtaddr(struct iommu_softc *iommu, paddr_t paddr)
{
	int i, sts;

	mtx_enter(&iommu->reg_lock);
	iommu_writeq(iommu, DMAR_RTADDR_REG, paddr);
	iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd | GCMD_SRTP);
	for (i = 0; i < 5; i++) {
		sts = iommu_readl(iommu, DMAR_GSTS_REG);
		if (sts & GSTS_RTPS)
			break;
	}
	mtx_leave(&iommu->reg_lock);

	if (i == 5) {
		printf("set_rtaddr fails\n");
	}
}

void *
iommu_alloc_page(struct iommu_softc *iommu, paddr_t *paddr)
{
	void	*va;

	*paddr = 0;
	va = km_alloc(VTD_PAGE_SIZE, &kv_page, &kp_zero, &kd_nowait);
	if (va == NULL) {
		panic("can't allocate page\n");
	}
	pmap_extract(pmap_kernel(), (vaddr_t)va, paddr);

	return (va);
}


void
iommu_issue_qi(struct iommu_softc *iommu, struct qi_entry *qi)
{
#if 0
	struct qi_entry *pi, *pw;

	idx = iommu->qi_head;
	pi = &iommu->qi[idx];
	pw = &iommu->qi[(idx+1) % MAXQ];
	iommu->qi_head = (idx+2) % MAXQ;

	memcpy(pw, &qi, sizeof(qi));
	issue command;
	while (pw->xxx)
		;
#endif
}

void
iommu_flush_tlb_qi(struct iommu_softc *iommu, int mode, int did)
{
	struct qi_entry qi;

	/* Use queued invalidation */
	qi.hi = 0;
	switch (mode) {
	case IOTLB_GLOBAL:
		qi.lo = QI_IOTLB | QI_IOTLB_IG_GLOBAL;
		break;
	case IOTLB_DOMAIN:
		qi.lo = QI_IOTLB | QI_IOTLB_IG_DOMAIN |
		    QI_IOTLB_DID(did);
		break;
	case IOTLB_PAGE:
		qi.lo = QI_IOTLB | QI_IOTLB_IG_PAGE | QI_IOTLB_DID(did);
		qi.hi = 0;
		break;
	}
	if (iommu->cap & CAP_DRD)
		qi.lo |= QI_IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		qi.lo |= QI_IOTLB_DW;
	iommu_issue_qi(iommu, &qi);
}

void
iommu_flush_ctx_qi(struct iommu_softc *iommu, int mode, int did,
    int sid, int fm)
{
	struct qi_entry qi;

	/* Use queued invalidation */
	qi.hi = 0;
	switch (mode) {
	case CTX_GLOBAL:
		qi.lo = QI_CTX | QI_CTX_IG_GLOBAL;
		break;
	case CTX_DOMAIN:
		qi.lo = QI_CTX | QI_CTX_IG_DOMAIN | QI_CTX_DID(did);
		break;
	case CTX_DEVICE:
		qi.lo = QI_CTX | QI_CTX_IG_DEVICE | QI_CTX_DID(did) |
		    QI_CTX_SID(sid) | QI_CTX_FM(fm);
		break;
	}
	iommu_issue_qi(iommu, &qi);
}

void
iommu_flush_write_buffer(struct iommu_softc *iommu)
{
	int i, sts;

	if (!(iommu->cap & CAP_RWBF))
		return;
	printf("writebuf\n");
	iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd | GCMD_WBF);
	for (i = 0; i < 5; i++) {
		sts = iommu_readl(iommu, DMAR_GSTS_REG);
		if (sts & GSTS_WBFS)
			break;
		delay(10000);
	}
	if (i == 5) {
		printf("write buffer flush fails\n");
	}
}

void
iommu_flush_cache(struct iommu_softc *iommu, void *addr, size_t size)
{
	if (!(iommu->ecap & ECAP_C))
		pmap_flush_cache((vaddr_t)addr, size);
}

void
iommu_flush_tlb(struct iommu_softc *iommu, int mode, int did)
{
	int		n;
	uint64_t	val;

	val = IOTLB_IVT;
	switch (mode) {
	case IOTLB_GLOBAL:
		val |= IIG_GLOBAL;
		break;
	case IOTLB_DOMAIN:
		val |= IIG_DOMAIN | IOTLB_DID(did);
		break;
	case IOTLB_PAGE:
		val |= IIG_PAGE | IOTLB_DID(did);
		break;
	}

	if (iommu->cap & CAP_DRD)
		val |= IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		val |= IOTLB_DW;

	mtx_enter(&iommu->reg_lock);

	iommu_writeq(iommu, DMAR_IOTLB_REG(iommu), val);
	n = 0;
	do {
		val = iommu_readq(iommu, DMAR_IOTLB_REG(iommu));
	} while (n++ < 5 && val & IOTLB_IVT);

	mtx_leave(&iommu->reg_lock);

#ifdef DEBUG
	{
		static int rg;
		int a, r;

		if (!rg) {
			a = (val >> IOTLB_IAIG_SHIFT) & IOTLB_IAIG_MASK;
			r = (val >> IOTLB_IIRG_SHIFT) & IOTLB_IIRG_MASK;
			if (a != r) {
				printf("TLB Requested:%d Actual:%d\n", r, a);
				rg = 1;
			}
		}
	}
#endif
}

void
iommu_flush_ctx(struct iommu_softc *iommu, int mode, int did, int sid, int fm)
{
	uint64_t	val;
	int		n;

	val = CCMD_ICC;
	switch (mode) {
	case CTX_GLOBAL:
		val |= CIG_GLOBAL;
		break;
	case CTX_DOMAIN:
		val |= CIG_DOMAIN | CCMD_DID(did);
		break;
	case CTX_DEVICE:
		val |= CIG_DEVICE | CCMD_DID(did) |
		    CCMD_SID(sid) | CCMD_FM(fm);
		break;
	}

	mtx_enter(&iommu->reg_lock);

	n = 0;
	iommu_writeq(iommu, DMAR_CCMD_REG, val);
	do {
		val = iommu_readq(iommu, DMAR_CCMD_REG);
	} while (n++ < 5 && val & CCMD_ICC);

	mtx_leave(&iommu->reg_lock);

#ifdef DEBUG
	{
		static int rg;
		int a, r;

		if (!rg) {
			a = (val >> CCMD_CAIG_SHIFT) & CCMD_CAIG_MASK;
			r = (val >> CCMD_CIRG_SHIFT) & CCMD_CIRG_MASK;
			if (a != r) {
				printf("CTX Requested:%d Actual:%d\n", r, a);
				rg = 1;
			}
		}
	}
#endif
}

void
iommu_enable_qi(struct iommu_softc *iommu, int enable)
{
	int	n = 0;
	int	sts;

	if (!(iommu->ecap & ECAP_QI))
		return;

	if (enable) {
		iommu->gcmd |= GCMD_QIE;

		mtx_enter(&iommu->reg_lock);

		iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd);
		do {
			sts = iommu_readl(iommu, DMAR_GSTS_REG);
		} while (n++ < 5 && !(sts & GSTS_QIES));

		mtx_leave(&iommu->reg_lock);

		printf("set.qie: %d\n", n);
	} else {
		iommu->gcmd &= ~GCMD_QIE;

		mtx_enter(&iommu->reg_lock);

		iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd);
		do {
			sts = iommu_readl(iommu, DMAR_GSTS_REG);
		} while (n++ < 5 && sts & GSTS_QIES);

		mtx_leave(&iommu->reg_lock);

		printf("clr.qie: %d\n", n);
	}
}

int
iommu_enable_translation(struct iommu_softc *iommu, int enable)
{
	uint32_t	sts;
	uint64_t	reg;
	int		n = 0;

	reg = 0;
	if (enable) {
		printf("enable iommu %d\n", iommu->id);
		iommu_showcfg(iommu, -1);

		iommu->gcmd |= GCMD_TE;

		/* Enable translation */
		printf(" pre tes: ");

		mtx_enter(&iommu->reg_lock);
		iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd);
		printf("xxx");
		do {
			printf("yyy");
			sts = iommu_readl(iommu, DMAR_GSTS_REG);
			delay(n * 10000);
		} while (n++ < 5 && !(sts & GSTS_TES));
		mtx_leave(&iommu->reg_lock);

		printf(" set.tes: %d\n", n);

		if (n >= 5) {
			printf("error.. unable to initialize iommu %d\n",
			    iommu->id);
			iommu->flags |= IOMMU_FLAGS_BAD;

			/* Disable IOMMU */
			iommu->gcmd &= ~GCMD_TE;
			mtx_enter(&iommu->reg_lock);
			iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd);
			mtx_leave(&iommu->reg_lock);

			return (1);
		}

		iommu_flush_ctx(iommu, CTX_GLOBAL, 0, 0, 0);
		iommu_flush_tlb(iommu, IOTLB_GLOBAL, 0);
	} else {
		iommu->gcmd &= ~GCMD_TE;

		mtx_enter(&iommu->reg_lock);

		iommu_writel(iommu, DMAR_GCMD_REG, iommu->gcmd);
		do {
			sts = iommu_readl(iommu, DMAR_GSTS_REG);
		} while (n++ < 5 && sts & GSTS_TES);
		mtx_leave(&iommu->reg_lock);

		printf(" clr.tes: %d\n", n);
	}

	return (0);
}

int
iommu_init(struct acpidmar_softc *sc, struct iommu_softc *iommu,
    struct acpidmar_drhd *dh)
{
	static int	niommu;
	int		len = VTD_PAGE_SIZE;
	int		i, gaw;
	uint32_t	sts;
	paddr_t		paddr;

	if (_bus_space_map(sc->sc_memt, dh->address, len, 0, &iommu->ioh) != 0) {
		return (-1);
	}

	TAILQ_INIT(&iommu->domains);
	iommu->id = ++niommu;
	iommu->flags = dh->flags;
	iommu->segment = dh->segment;
	iommu->iot = sc->sc_memt;

	iommu->cap = iommu_readq(iommu, DMAR_CAP_REG);
	iommu->ecap = iommu_readq(iommu, DMAR_ECAP_REG);
	iommu->ndoms = cap_nd(iommu->cap);

	printf("  caps: %s%s%s%s%s%s%s%s%s%s%s\n",
	    iommu->cap & CAP_AFL ? "afl " : "",		// adv fault
	    iommu->cap & CAP_RWBF ? "rwbf " : "",	// write-buffer flush
	    iommu->cap & CAP_PLMR ? "plmr " : "",	// protected lo region
	    iommu->cap & CAP_PHMR ? "phmr " : "",	// protected hi region
	    iommu->cap & CAP_CM ? "cm " : "",		// caching mode
	    iommu->cap & CAP_ZLR ? "zlr " : "",		// zero-length read
	    iommu->cap & CAP_PSI ? "psi " : "",		// page invalidate
	    iommu->cap & CAP_DWD ? "dwd " : "",		// write drain
	    iommu->cap & CAP_DRD ? "drd " : "",		// read drain
	    iommu->cap & CAP_FL1GP ? "Gb " : "",	// 1Gb pages
	    iommu->cap & CAP_PI ? "pi " : "");		// posted interrupts
	printf("  ecap: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
	    iommu->ecap & ECAP_C ? "c " : "",		// coherent
	    iommu->ecap & ECAP_QI ? "qi " : "",		// queued invalidate
	    iommu->ecap & ECAP_DT ? "dt " : "",		// device iotlb
	    iommu->ecap & ECAP_IR ? "ir " : "",		// intr remap
	    iommu->ecap & ECAP_EIM ? "eim " : "",	// x2apic
	    iommu->ecap & ECAP_PT ? "pt " : "",		// passthrough
	    iommu->ecap & ECAP_SC ? "sc " : "",		// snoop control
	    iommu->ecap & ECAP_ECS ? "ecs " : "",	// extended context
	    iommu->ecap & ECAP_MTS ? "mts " : "",	// memory type
	    iommu->ecap & ECAP_NEST ? "nest " : "",	// nested translations
	    iommu->ecap & ECAP_DIS ? "dis " : "",	// deferred invalidation
	    iommu->ecap & ECAP_PASID ? "pas " : "",	// pasid
	    iommu->ecap & ECAP_PRS ? "prs " : "",	// page request
	    iommu->ecap & ECAP_ERS ? "ers " : "",	// execute request
	    iommu->ecap & ECAP_SRS ? "srs " : "",	// supervisor request
	    iommu->ecap & ECAP_NWFS ? "nwfs " : "",	// no write flag
	    iommu->ecap & ECAP_EAFS ? "eafs " : "");	// extended accessed flag

	mtx_init(&iommu->reg_lock, IPL_HIGH);

	/* Clear Interrupt Masking */
	iommu_writel(iommu, DMAR_FSTS_REG, FSTS_PFO | FSTS_PPF);

	iommu->intr = acpidmar_intr_establish(iommu, IPL_HIGH,
	    acpidmar_intr, iommu, "dmarintr");

	/* Enable interrupts */
	sts = iommu_readl(iommu, DMAR_FECTL_REG);
	iommu_writel(iommu, DMAR_FECTL_REG, sts & ~FECTL_IM);

	/* Allocate root pointer */
	iommu->root = iommu_alloc_page(iommu, &paddr);
#ifdef DEBUG
	printf("Allocated root pointer: pa:%.16llx va:%p\n",
	    (uint64_t)paddr, iommu->root);
#endif
	iommu->rtaddr = paddr;
	iommu_flush_write_buffer(iommu);
	iommu_set_rtaddr(iommu, paddr);

#if 0
	if (iommu->ecap & ECAP_QI) {
		/* Queued Invalidation support */
		iommu->qi = iommu_alloc_page(iommu, &iommu->qip);
		iommu_writeq(iommu, DMAR_IQT_REG, 0);
		iommu_writeq(iommu, DMAR_IQA_REG, iommu->qip | IQA_QS_256);
	}
	if (iommu->ecap & ECAP_IR) {
		/* Interrupt remapping support */
		iommu_writeq(iommu, DMAR_IRTA_REG, 0);
	}
#endif

	gaw = -1;
	iommu->mgaw = cap_mgaw(iommu->cap);
	printf("gaw: %d { ", iommu->mgaw);
	for (i = 0; i < 5; i++) {
		if (cap_sagaw(iommu->cap) & (1L << i)) {
			gaw = VTD_LEVELTOAW(i);
			printf("%d ", gaw);
			iommu->agaw = gaw;
		}
	}
	printf("}\n");

	sts = iommu_readl(iommu, DMAR_GSTS_REG);
	if (sts & GSTS_TES)
		iommu->gcmd |= GCMD_TE;
	if (sts & GSTS_QIES)
		iommu->gcmd |= GCMD_QIE;
	if (sts & GSTS_IRES)
		iommu->gcmd |= GCMD_IRE;
	if (iommu->gcmd) {
		printf("gcmd: %x preset\n", iommu->gcmd);
	}
	acpidmar_intr(iommu);
	return (0);
}

const char *dmar_rn(int reg);

const char *
dmar_rn(int reg)
{
	switch (reg) {
	case DMAR_VER_REG: return "ver";
	case DMAR_CAP_REG: return "cap";
	case DMAR_ECAP_REG: return "ecap";
	case DMAR_GSTS_REG: return "gsts";
	case DMAR_GCMD_REG: return "gcmd";
	case DMAR_FSTS_REG: return "fsts";
	case DMAR_FECTL_REG: return "fectl";
	case DMAR_RTADDR_REG: return "rtaddr";
	case DMAR_FEDATA_REG: return "fedata";
	case DMAR_FEADDR_REG: return "feaddr";
	case DMAR_FEUADDR_REG: return "feuaddr";
	case DMAR_PMEN_REG: return "pmen";
	case DMAR_IEDATA_REG: return "iedata";
	case DMAR_IEADDR_REG: return "ieaddr";
	case DMAR_IEUADDR_REG: return "ieuaddr";
	case DMAR_IRTA_REG: return "irta";
	case DMAR_CCMD_REG: return "ccmd";
	case DMAR_IQH_REG: return "iqh";
	case DMAR_IQT_REG: return "iqt";
	case DMAR_IQA_REG: return "iqa";
	}
	return "unknown";
}

/* Read/Write IOMMU register */
uint32_t
iommu_readl(struct iommu_softc *iommu, int reg)
{
	uint32_t	v;

	v = bus_space_read_4(iommu->iot, iommu->ioh, reg);
	if (reg < 00) {
		printf("iommu%d: read %x %.8lx [%s]\n",
		    iommu->id, reg, (unsigned long)v, dmar_rn(reg));
	}

	return (v);
}

void
iommu_writel(struct iommu_softc *iommu, int reg, uint32_t v)
{
	if (reg < 00) {
		printf("iommu%d: write %x %.8lx [%s]\n",
		    iommu->id, reg, (unsigned long)v, dmar_rn(reg));
	}
	bus_space_write_4(iommu->iot, iommu->ioh, reg, (uint32_t)v);
}

uint64_t
iommu_readq(struct iommu_softc *iommu, int reg)
{
	uint64_t	v;

	v = bus_space_read_8(iommu->iot, iommu->ioh, reg);
	if (reg < 00) {
		printf("iommu%d: read %x %.8lx [%s]\n",
		    iommu->id, reg, (unsigned long)v, dmar_rn(reg));
	}

	return (v);
}

void
iommu_writeq(struct iommu_softc *iommu, int reg, uint64_t v)
{
	if (reg < 00) {
		printf("iommu%d: write %x %.8lx [%s]\n",
		    iommu->id, reg, (unsigned long)v, dmar_rn(reg));
	}
	bus_space_write_8(iommu->iot, iommu->ioh, reg, v);
}

/* Check if a device is within a device scope */
int
acpidmar_match_devscope(struct devlist_head *devlist, pci_chipset_tag_t pc,
    int sid)
{
	struct dmar_devlist	*ds;
	int			sub, sec, i;
	int			bus, dev, fun, sbus;
	pcireg_t		reg;
	pcitag_t		tag;

	sbus = sid_bus(sid);
	TAILQ_FOREACH(ds, devlist, link) {
		bus = ds->bus;
		dev = ds->dp[0].device;
		fun = ds->dp[0].function;
		/* Walk PCI bridges in path */
		for (i = 1; i < ds->ndp; i++) {
			tag = pci_make_tag(pc, bus, dev, fun);
			reg = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
			bus = PPB_BUSINFO_SECONDARY(reg);
			dev = ds->dp[i].device;
			fun = ds->dp[i].function;
		}

		/* Check for device exact match */
		if (sid == mksid(bus, dev, fun)) {
			return DMAR_ENDPOINT;
		}

		/* Check for device subtree match */
		if (ds->type == DMAR_BRIDGE) {
			tag = pci_make_tag(pc, bus, dev, fun);
			reg = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
			sec = PPB_BUSINFO_SECONDARY(reg);
			sub = PPB_BUSINFO_SUBORDINATE(reg);
			if (sec <= sbus && sbus <= sub) {
				return DMAR_BRIDGE;
			}
		}
	}

	return (0);
}

struct domain *
domain_create(struct iommu_softc *iommu, int did)
{
	struct domain	*dom;
	int gaw;

	printf("iommu%d: create domain: %.4x\n", iommu->id, did);
	dom = malloc(sizeof(*dom), M_DEVBUF, M_ZERO | M_WAITOK);
	dom->did = did;
	dom->iommu = iommu;
	dom->pte = iommu_alloc_page(iommu, &dom->ptep);
	TAILQ_INIT(&dom->devices);

	/* Setup DMA */
	dom->dmat._cookie = dom;
	dom->dmat._dmamap_create    = dmar_dmamap_create;	// nop
	dom->dmat._dmamap_destroy   = dmar_dmamap_destroy;	// nop
	dom->dmat._dmamap_load      = dmar_dmamap_load;		// lm
	dom->dmat._dmamap_load_mbuf = dmar_dmamap_load_mbuf;	// lm
	dom->dmat._dmamap_load_uio  = dmar_dmamap_load_uio;	// lm
	dom->dmat._dmamap_load_raw  = dmar_dmamap_load_raw;	// lm
	dom->dmat._dmamap_unload    = dmar_dmamap_unload;	// um
	dom->dmat._dmamap_sync      = dmar_dmamap_sync;		// lm
	dom->dmat._dmamem_alloc     = dmar_dmamem_alloc;	// nop
	dom->dmat._dmamem_free      = dmar_dmamem_free;		// nop
	dom->dmat._dmamem_map       = dmar_dmamem_map;		// nop
	dom->dmat._dmamem_unmap     = dmar_dmamem_unmap;	// nop
	dom->dmat._dmamem_mmap      = dmar_dmamem_mmap;

	snprintf(dom->exname, sizeof(dom->exname), "did:%x.%.4x",
	    iommu->id, dom->did);

	/* Setup IOMMU address map */
	gaw = min(iommu->agaw, iommu->mgaw);
	printf("Creating Domain with %d bits\n", gaw);
	dom->iovamap = extent_create(dom->exname, 1024*1024*16,
	    (1LL << gaw)-1,
	    M_DEVBUF, NULL, 0,
	    EX_WAITOK|EX_NOCOALESCE);

	/* Zero out Interrupt region */
	extent_alloc_region(dom->iovamap, 0xFEE00000L, 0x100000,
	    EX_WAITOK);
	mtx_init(&dom->exlck, IPL_HIGH);

	TAILQ_INSERT_TAIL(&iommu->domains, dom, link);

	return dom;
}

void domain_add_device(struct domain *dom, int sid)
{	
	struct domain_dev *ddev;
	
	printf("add %s to iommu%d.%.4x\n", dmar_bdf(sid), dom->iommu->id, dom->did);
	ddev = malloc(sizeof(*ddev), M_DEVBUF, M_ZERO | M_WAITOK);
	ddev->sid = sid;
	TAILQ_INSERT_TAIL(&dom->devices, ddev, link);

	/* Should set context entry here?? */
}

void domain_remove_device(struct domain *dom, int sid)
{
	struct domain_dev *ddev, *tmp;

	TAILQ_FOREACH_SAFE(ddev, &dom->devices, link, tmp) {
		if (ddev->sid == sid) {
			TAILQ_REMOVE(&dom->devices, ddev, link);
			free(ddev, sizeof(*ddev), M_DEVBUF);
		}
	}
}

/* Lookup domain by segment & source id (bus.device.function) */
struct domain *
domain_lookup(struct acpidmar_softc *sc, int segment, int sid)
{
	struct iommu_softc	*iommu;
	struct domain_dev 	*ddev;
	struct domain		*dom;
	int			rc;

	if (sc == NULL) {
		return NULL;
	}

	/* Lookup IOMMU for this device */
	TAILQ_FOREACH(iommu, &sc->sc_drhds, link) {
		if (iommu->segment != segment)
			continue;
		/* Check for devscope match or catchall iommu */
		rc = acpidmar_match_devscope(&iommu->devices, sc->sc_pc, sid);
		if (rc != 0 || iommu->flags) {
			break;
		}
	}
	if (!iommu) {
		printf("%s: no iommu found\n", dmar_bdf(sid));
		return NULL;
	}

	acpidmar_intr(iommu);

	/* Search domain devices */
	TAILQ_FOREACH(dom, &iommu->domains, link) {
		TAILQ_FOREACH(ddev, &dom->devices, link) {
			/* XXX: match all functions? */
			if (ddev->sid == sid) {
				return dom;
			}
		}
	}
	if (iommu->ndoms <= 2) {
		/* Running out of domains.. create catchall domain */
		if (!iommu->unity) {
			iommu->unity = domain_create(iommu, 1);
		}
		dom = iommu->unity;
	} else {
		dom = domain_create(iommu, --iommu->ndoms);
	}
	if (!dom) {
		printf("no domain here\n");
		return NULL;
	}

	/* Add device to domain */
	domain_add_device(dom, sid);

	return dom;
}

void  _iommu_map(void *dom, vaddr_t va, bus_addr_t gpa, bus_size_t len)
{
	bus_size_t i;
	paddr_t hpa;

	if (dom == NULL) {
		return;
	}
	printf("Mapping dma: %lx = %lx/%lx\n", va, gpa, len);
	for (i = 0; i < len; i += PAGE_SIZE) {
		hpa = 0;
		pmap_extract(curproc->p_vmspace->vm_map.pmap, va, &hpa);
		if (i < 25 * PAGE_SIZE) {
			printf("  hpa: %lx %lx\n", gpa, hpa);
		}
		domain_map_page(dom, gpa, hpa, PTE_P | PTE_R | PTE_W);
		gpa += PAGE_SIZE;
		va  += PAGE_SIZE;
	}
}

void *_iommu_domain(int segment, int bus, int dev, int func, int *id)
{
	struct domain *dom;

	dom = domain_lookup(acpidmar_sc, segment, mksid(bus, dev, func));
	if (dom) {
		*id = dom->did;
	}
	return dom;
}

void domain_map_device(struct domain *dom, int sid);

void
domain_map_device(struct domain *dom, int sid)
{
	struct iommu_softc	*iommu;
	struct context_entry	*ctx;
	paddr_t			paddr;
	int			bus, devfn;
	int			tt, lvl;

	iommu = dom->iommu;

	bus = sid_bus(sid);
	devfn = sid_devfn(sid);

	/* Create Bus mapping */
	if (!root_entry_is_valid(&iommu->root[bus])) {
		iommu->ctx[bus] = iommu_alloc_page(iommu, &paddr);
		iommu->root[bus].lo = paddr | ROOT_P;
		iommu_flush_cache(iommu, &iommu->root[bus],
		    sizeof(struct root_entry));
		dprintf("iommu%d: Allocate context for bus: %.2x pa:%.16llx va:%p\n",
		    iommu->id, bus, (uint64_t)paddr,
		    iommu->ctx[bus]);
	}

	/* Create DevFn mapping */
	ctx = iommu->ctx[bus] + devfn;
	if (!context_entry_is_valid(ctx)) {
		tt = CTX_T_MULTI;
		lvl = VTD_AWTOLEVEL(iommu->agaw);

		/* Initialize context */
		context_set_slpte(ctx, dom->ptep);
		context_set_translation_type(ctx, tt);
		context_set_domain_id(ctx, dom->did);
		context_set_address_width(ctx, lvl);
		context_set_present(ctx);

		/* Flush it */
		iommu_flush_cache(iommu, ctx, sizeof(struct context_entry));
		if ((iommu->cap & CAP_CM) || force_cm) {
			iommu_flush_ctx(iommu, CTX_DEVICE, dom->did, sid, 0);
			iommu_flush_tlb(iommu, IOTLB_GLOBAL, 0);
		} else {
			iommu_flush_write_buffer(iommu);
		}
		dprintf("iommu%d: %s set context ptep:%.16llx lvl:%d did:%.4x tt:%d\n",
		    iommu->id, dmar_bdf(sid), (uint64_t)dom->ptep, lvl,
		    dom->did, tt);
	}
}

struct domain *
acpidmar_pci_attach(struct acpidmar_softc *sc, int segment, int sid, int mapctx)
{
	static struct domain	*dom;

	dom = domain_lookup(sc, segment, sid);
	if (!dom) {
		printf("no domain: %s\n", dmar_bdf(sid));
		return NULL;
	}

	if (mapctx)
		domain_map_device(dom, sid);

	return dom;
}

void
acpidmar_pci_hook(pci_chipset_tag_t pc, struct pci_attach_args *pa)
{
	int		bus, dev, fun;
	struct domain	*dom;
	pcireg_t	reg;

	if (!acpidmar_sc) {
		/* No DMAR, ignore */
		return;
	}
	pci_decompose_tag(pc, pa->pa_tag, &bus, &dev, &fun);
	reg = pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG);
#if 0
	if (PCI_CLASS(reg) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_DISPLAY_VGA) {
		printf("dmar: %.4x:%.2x:%.2x.%x is VGA, ignoring\n",
		    pa->pa_domain, bus, dev, fun);
		return;
	}
#endif
	/* Add device to domain */
	dom = acpidmar_pci_attach(acpidmar_sc, pa->pa_domain,
		mksid(bus, dev, fun), 0);
	if (dom == NULL)
		return;

	if (PCI_CLASS(reg) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_DISPLAY_VGA) {
		dom->flag = DOM_DEBUG | DOM_NOMAP;
	}
	if (PCI_CLASS(reg) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_BRIDGE_ISA) {
		/* For ISA Bridges, map 0-16Mb as 1:1 */
		printf("dmar: %.4x:%.2x:%.2x.%x mapping ISA\n",
		    pa->pa_domain, bus, dev, fun);
		domain_map_pthru(dom, 0x00, 16*1024*1024);
	}
	/* Change DMA tag */
	pa->pa_dmat = &dom->dmat;
}

/* Create list of device scope entries from ACPI table */
void
acpidmar_parse_devscope(union acpidmar_entry *de, int off, int segment,
    struct devlist_head *devlist)
{
	struct acpidmar_devscope	*ds;
	struct dmar_devlist		*d;
	int				dplen, i;

	TAILQ_INIT(devlist);
	while (off < de->length) {
		ds = (struct acpidmar_devscope *)((unsigned char *)de + off);
		off += ds->length;

		/* We only care about bridges and endpoints */
		if (ds->type != DMAR_ENDPOINT && ds->type != DMAR_BRIDGE)
			continue;

		dplen = ds->length - sizeof(*ds);
		d = malloc(sizeof(*d) + dplen, M_DEVBUF, M_ZERO | M_WAITOK);
		d->bus  = ds->bus;
		d->type = ds->type;
		d->ndp  = dplen / 2;
		d->dp   = (void *)&d[1];
		memcpy(d->dp, &ds[1], dplen);
		TAILQ_INSERT_TAIL(devlist, d, link);

		printf("  %8s  %.4x:%.2x.%.2x.%x {",
		    ds->type == DMAR_BRIDGE ? "bridge" : "endpoint",
		    segment, ds->bus,
		    d->dp[0].device,
		    d->dp[0].function);

		for (i = 1; i < d->ndp; i++) {
			printf(" %2x.%x ",
			    d->dp[i].device,
			    d->dp[i].function);
		}
		printf("}\n");
	}
}

/* DMA Remapping Hardware Unit */
void
acpidmar_drhd(struct acpidmar_softc *sc, union acpidmar_entry *de)
{
	struct iommu_softc	*iommu;

	printf("DRHD: segment:%.4x base:%.16llx flags:%.2x\n",
	    de->drhd.segment,
	    de->drhd.address,
	    de->drhd.flags);
	iommu = malloc(sizeof(*iommu), M_DEVBUF, M_ZERO | M_WAITOK);
	acpidmar_parse_devscope(de, sizeof(de->drhd), de->drhd.segment,
	    &iommu->devices);
	iommu_init(sc, iommu, &de->drhd);

	if (de->drhd.flags) {
		/* Catchall IOMMU goes at end of list */
		TAILQ_INSERT_TAIL(&sc->sc_drhds, iommu, link);
	} else {
		TAILQ_INSERT_HEAD(&sc->sc_drhds, iommu, link);
	}
}

/* Reserved Memory Region Reporting */
void
acpidmar_rmrr(struct acpidmar_softc *sc, union acpidmar_entry *de)
{
	struct rmrr_softc	*rmrr;
	bios_memmap_t		*im, *jm;
	uint64_t		start, end;

	printf("RMRR: segment:%.4x range:%.16llx-%.16llx\n",
	    de->rmrr.segment, de->rmrr.base, de->rmrr.limit);
	if (de->rmrr.limit <= de->rmrr.base) {
		printf("  buggy BIOS\n");
		return;
	}

	rmrr = malloc(sizeof(*rmrr), M_DEVBUF, M_ZERO | M_WAITOK);
	rmrr->start = trunc_page(de->rmrr.base);
	rmrr->end = round_page(de->rmrr.limit);
	rmrr->segment = de->rmrr.segment;
	acpidmar_parse_devscope(de, sizeof(de->rmrr), de->rmrr.segment,
	    &rmrr->devices);

	for (im = bios_memmap; im->type != BIOS_MAP_END; im++) {
		if (im->type != BIOS_MAP_RES)
			continue;
		/* Search for adjacent reserved regions */
		start = im->addr;
		end   = im->addr+im->size;
		for (jm = im+1; jm->type == BIOS_MAP_RES && end == jm->addr;
		    jm++) {
			end = jm->addr+jm->size;
		}
		printf("e820: %.16llx - %.16llx\n", start, end);
		if (start <= rmrr->start && rmrr->end <= end) {
			/* Bah.. some buggy BIOS stomp outside RMRR */
			printf("  ** inside E820 Reserved %.16llx %.16llx\n",
			    start, end);
			rmrr->start = trunc_page(start);
			rmrr->end   = round_page(end);
			break;
		}
	}
	TAILQ_INSERT_TAIL(&sc->sc_rmrrs, rmrr, link);
}

/* Root Port ATS Reporting */
void
acpidmar_atsr(struct acpidmar_softc *sc, union acpidmar_entry *de)
{
	struct atsr_softc *atsr;

	printf("ATSR: segment:%.4x flags:%x\n",
	    de->atsr.segment,
	    de->atsr.flags);

	atsr = malloc(sizeof(*atsr), M_DEVBUF, M_ZERO | M_WAITOK);
	atsr->flags = de->atsr.flags;
	atsr->segment = de->atsr.segment;
	acpidmar_parse_devscope(de, sizeof(de->atsr), de->atsr.segment,
	    &atsr->devices);

	TAILQ_INSERT_TAIL(&sc->sc_atsrs, atsr, link);
}

void
acpidmar_init(struct acpidmar_softc *sc, struct acpi_dmar *dmar)
{
	struct rmrr_softc	*rmrr;
	struct iommu_softc	*iommu;
	struct domain		*dom;
	struct dmar_devlist	*dl;
	union acpidmar_entry	*de;
	int			off, sid, rc;

	printf(": hardware width: %d, intr_remap:%d x2apic_opt_out:%d\n",
	    dmar->haw+1,
	    !!(dmar->flags & 0x1),
	    !!(dmar->flags & 0x2));
	sc->sc_haw = dmar->haw+1;
	sc->sc_flags = dmar->flags;

	TAILQ_INIT(&sc->sc_drhds);
	TAILQ_INIT(&sc->sc_rmrrs);
	TAILQ_INIT(&sc->sc_atsrs);

	off = sizeof(*dmar);
	while (off < dmar->hdr.length) {
		de = (union acpidmar_entry *)((unsigned char *)dmar + off);
		switch (de->type) {
		case DMAR_DRHD:
			acpidmar_drhd(sc, de);
			break;
		case DMAR_RMRR:
			acpidmar_rmrr(sc, de);
			break;
		case DMAR_ATSR:
			acpidmar_atsr(sc, de);
			break;
		default:
			printf("DMAR: unknown %x\n", de->type);
			break;
		}
		off += de->length;
	}

	/* Pre-create domains for iommu devices */
	TAILQ_FOREACH(iommu, &sc->sc_drhds, link) {
		TAILQ_FOREACH(dl, &iommu->devices, link) {
			sid = mksid(dl->bus, dl->dp[0].device,
			    dl->dp[0].function);
			dom = acpidmar_pci_attach(sc, iommu->segment, sid, 0);
			if (dom != NULL) {
				printf("%.4x:%.2x:%.2x.%x iommu:%d did:%.4x\n",
				    iommu->segment, dl->bus, dl->dp[0].device, dl->dp[0].function,
				    iommu->id, dom->did);
			}
		}
	}
	/* Map passthrough pages for RMRR */
	TAILQ_FOREACH(rmrr, &sc->sc_rmrrs, link) {
		TAILQ_FOREACH(dl, &rmrr->devices, link) {
			sid = mksid(dl->bus, dl->dp[0].device,
			    dl->dp[0].function);
			dom = acpidmar_pci_attach(sc, rmrr->segment, sid, 0);
			if (dom != NULL) {
				printf("%s map ident: %.16llx %.16llx\n",
				    dom_bdf(dom), rmrr->start, rmrr->end);
				domain_map_pthru(dom, rmrr->start, rmrr->end);
				rc = extent_alloc_region(dom->iovamap,
				    rmrr->start, rmrr->end, EX_WAITOK);
			}
		}
	}
	printf("========\n");
}

int
acpidmar_activate(struct device *self, int act)
{
	struct acpidmar_softc *sc = (struct acpidmar_softc *)self;
	struct iommu_softc *iommu;

	printf("called acpidmar_activate %d %p\n", act, sc);

	if (sc == NULL) {
		return (0);
	}

	switch (act) {
	case DVACT_RESUME:
		TAILQ_FOREACH(iommu, &sc->sc_drhds, link) {
			printf("iommu%d resume\n", iommu->id);
			iommu_flush_write_buffer(iommu);
			iommu_set_rtaddr(iommu, iommu->rtaddr);
			iommu_writel(iommu, DMAR_FEDATA_REG, iommu->fedata);
			iommu_writel(iommu, DMAR_FEADDR_REG, iommu->feaddr);
			iommu_writel(iommu, DMAR_FEUADDR_REG,
			    iommu->feaddr >> 32);
			if ((iommu->flags & (IOMMU_FLAGS_BAD|IOMMU_FLAGS_SUSPEND)) ==
			    IOMMU_FLAGS_SUSPEND) {
				printf("enable wakeup translation\n");
				iommu_enable_translation(iommu, 1);
			}
			iommu_showcfg(iommu, -1);
		}
		break;
	case DVACT_SUSPEND:
		TAILQ_FOREACH(iommu, &sc->sc_drhds, link) {
			printf("iommu%d suspend\n", iommu->id);
			if (iommu->flags & IOMMU_FLAGS_BAD)
				continue;
			iommu->flags |= IOMMU_FLAGS_SUSPEND;
			iommu_enable_translation(iommu, 0);
			iommu_showcfg(iommu, -1);
		}
		break;
	}
	return (0);
}

void
acpidmar_sw(int act)
{
	if (acpidmar_sc)
		acpidmar_activate((void*)acpidmar_sc, act);
}

int
acpidmar_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args		*aaa = aux;
	struct acpi_table_header	*hdr;

	/* If we do not have a table, it is not us */
	if (aaa->aaa_table == NULL)
		return (0);

	/* If it is an DMAR table, we can attach */
	hdr = (struct acpi_table_header *)aaa->aaa_table;
	if (memcmp(hdr->signature, DMAR_SIG, sizeof(DMAR_SIG) - 1) != 0)
		return (0);

	return (1);
}

void
acpidmar_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpidmar_softc	*sc = (void *)self;
	struct acpi_attach_args	*aaa = aux;
	struct acpi_dmar	*dmar = (struct acpi_dmar *)aaa->aaa_table;

	acpidmar_sc = sc;
	sc->sc_memt = aaa->aaa_memt;
	acpidmar_init(sc, dmar);
}

/* Interrupt shiz */
void acpidmar_msi_hwmask(struct pic *, int);
void acpidmar_msi_hwunmask(struct pic *, int);
void acpidmar_msi_addroute(struct pic *, struct cpu_info *, int, int, int);
void acpidmar_msi_delroute(struct pic *, struct cpu_info *, int, int, int);

void
acpidmar_msi_hwmask(struct pic *pic, int pin)
{
	struct iommu_pic	*ip = (void *)pic;
	struct iommu_softc	*iommu = ip->iommu;

	printf("msi_hwmask\n");

	mtx_enter(&iommu->reg_lock);

	iommu_writel(iommu, DMAR_FECTL_REG, FECTL_IM);
	iommu_readl(iommu, DMAR_FECTL_REG);

	mtx_leave(&iommu->reg_lock);
}

void
acpidmar_msi_hwunmask(struct pic *pic, int pin)
{
	struct iommu_pic	*ip = (void *)pic;
	struct iommu_softc	*iommu = ip->iommu;

	printf("msi_hwunmask\n");

	mtx_enter(&iommu->reg_lock);

	iommu_writel(iommu, DMAR_FECTL_REG, 0);
	iommu_readl(iommu, DMAR_FECTL_REG);

	mtx_leave(&iommu->reg_lock);
}

void
acpidmar_msi_addroute(struct pic *pic, struct cpu_info *ci, int pin, int vec,
    int type)
{
	struct iommu_pic	*ip = (void *)pic;
	struct iommu_softc	*iommu = ip->iommu;

	mtx_enter(&iommu->reg_lock);

	iommu->fedata = vec;
	iommu->feaddr = 0xfee00000L | (ci->ci_apicid << 12);
	iommu_writel(iommu,  DMAR_FEDATA_REG, vec);
	iommu_writel(iommu, DMAR_FEADDR_REG, iommu->feaddr);
	iommu_writel(iommu, DMAR_FEUADDR_REG, iommu->feaddr >> 32);

	mtx_leave(&iommu->reg_lock);
}

void
acpidmar_msi_delroute(struct pic *pic, struct cpu_info *ci, int pin, int vec,
    int type)
{
	printf("msi_delroute\n");
}

void *
acpidmar_intr_establish(void *ctx, int level, int (*func)(void *),
    void *arg, const char *what)
{
	struct iommu_softc	*iommu = ctx;
	struct pic		*pic;

	pic = &iommu->pic.pic;
	iommu->pic.iommu = iommu;

	strlcpy(pic->pic_dev.dv_xname, "dmarpic",
		sizeof(pic->pic_dev.dv_xname));
	pic->pic_type = PIC_MSI;
	pic->pic_hwmask = acpidmar_msi_hwmask;
	pic->pic_hwunmask = acpidmar_msi_hwunmask;
	pic->pic_addroute = acpidmar_msi_addroute;
	pic->pic_delroute = acpidmar_msi_delroute;
	pic->pic_edge_stubs = ioapic_edge_stubs;
#ifdef MULTIPROCESSOR
	mtx_init(&pic->pic_mutex, level);
#endif

	return intr_establish(-1, pic, 0, IST_PULSE, level, func, arg, what);
}

int
acpidmar_intr(void *ctx)
{
	struct iommu_softc		*iommu = ctx;
	struct fault_entry		fe;
	static struct fault_entry	ofe;
	int				fro, nfr, fri, i;
	uint32_t			sts;

	//splassert(IPL_HIGH);

	if (!(iommu->gcmd & GCMD_TE)) {
		return (1);
	}
	mtx_enter(&iommu->reg_lock);
	sts = iommu_readl(iommu, DMAR_FECTL_REG);
	sts = iommu_readl(iommu, DMAR_FSTS_REG);

	if (!(sts & FSTS_PPF)) {
		mtx_leave(&iommu->reg_lock);
		return (1);
	}

	nfr = cap_nfr(iommu->cap);
	fro = cap_fro(iommu->cap);
	fri = (sts >> FSTS_FRI_SHIFT) & FSTS_FRI_MASK;
	for (i = 0; i < nfr; i++) {
		fe.hi = iommu_readq(iommu, fro + (fri*16) + 8);
		if (!(fe.hi & FRCD_HI_F))
			break;

		fe.lo = iommu_readq(iommu, fro + (fri*16));
		if (ofe.hi != fe.hi || ofe.lo != fe.lo) {
			iommu_showfault(iommu, fri, &fe);
			ofe.hi = fe.hi;
			ofe.lo = fe.lo;
		}
		fri = (fri + 1) % nfr;
	}

	iommu_writel(iommu, DMAR_FSTS_REG, FSTS_PFO | FSTS_PPF);

	mtx_leave(&iommu->reg_lock);

	return (1);
}

const char *vtd_faults[] = {
	"Software",
	"Root Entry Not Present",	/* ok (rtaddr + 4096) */
	"Context Entry Not Present",	/* ok (no CTX_P) */
	"Context Entry Invalid",	/* ok (tt = 3) */
	"Address Beyond MGAW",
	"Write",			/* ok */
	"Read",				/* ok */
	"Paging Entry Invalid",		/* ok */
	"Root Table Invalid",
	"Context Table Invalid",
	"Root Entry Reserved",          /* ok (root.lo |= 0x4) */
	"Context Entry Reserved",
	"Paging Entry Reserved",
	"Context Entry TT",
	"Reserved",
};

void iommu_showpte(uint64_t, int, uint64_t);

void
iommu_showpte(uint64_t ptep, int lvl, uint64_t base)
{
	uint64_t nb, pb, i;
	struct pte_entry *pte;

	pte = (void *)PMAP_DIRECT_MAP(ptep);
	for (i = 0; i < 512; i++) {
		if (!(pte[i].val & PTE_P))
			continue;
		nb = base + (i << lvl);
		pb = pte[i].val & ~VTD_PAGE_MASK;
		if(lvl == VTD_LEVEL0) {
			printf("   %3llx %.16llx = %.16llx %c%c %s\n",
			    i, nb, pb,
			    pte[i].val == PTE_R ? 'r' : ' ',
			    pte[i].val & PTE_W ? 'w' : ' ',
			    (nb == pb) ? " ident" : "");
			if (nb == pb)
				return;
		} else {
			iommu_showpte(pb, lvl - VTD_STRIDE_SIZE, nb);
		}
	}
}

void
iommu_showcfg(struct iommu_softc *iommu, int sid)
{
	int i, j, sts, cmd;
	struct context_entry *ctx;
	pcitag_t tag;
	pcireg_t clc;

	cmd = iommu_readl(iommu, DMAR_GCMD_REG);
	sts = iommu_readl(iommu, DMAR_GSTS_REG);
	printf("iommu%d: flags:%d root pa:%.16llx %s %s %s %.8x %.8x\n",
	    iommu->id, iommu->flags, iommu_readq(iommu, DMAR_RTADDR_REG),
	    sts & GSTS_TES ? "enabled" : "disabled",
	    sts & GSTS_QIES ? "qi" : "ccmd",
	    sts & GSTS_IRES ? "ir" : "",
	    cmd, sts);
	for (i = 0; i < 256; i++) {
		if (!root_entry_is_valid(&iommu->root[i])) {
			continue;
		}
		for (j = 0; j < 256; j++) {
			ctx = iommu->ctx[i] + j;
			if (!context_entry_is_valid(ctx)) {
				continue;
			}
			tag = pci_make_tag(NULL, i, (j >> 3), j & 0x7);
			clc = pci_conf_read(NULL, tag, 0x08) >> 8;
			printf("  %.2x:%.2x.%x lvl:%d did:%.4x tt:%d ptep:%.16llx flag:%x cc:%.6x\n",
			    i, (j >> 3), j & 7,
			    context_address_width(ctx),
			    context_domain_id(ctx),
			    context_translation_type(ctx),
			    context_pte(ctx),
			    context_user(ctx),
			    clc);
#if 0
			/* dump pagetables */
			iommu_showpte(ctx->lo & ~VTD_PAGE_MASK, iommu->agaw -
			    VTD_STRIDE_SIZE, 0);
#endif
		}
	}
}

void
iommu_showfault(struct iommu_softc *iommu, int fri, struct fault_entry *fe)
{
	int bus, dev, fun, type, fr, df;
	bios_memmap_t	*im;
	const char *mapped;

	if (!(fe->hi & FRCD_HI_F))
		return;
	type = (fe->hi & FRCD_HI_T) ? 'r' : 'w';
	fr = (fe->hi >> FRCD_HI_FR_SHIFT) & FRCD_HI_FR_MASK;
	bus = (fe->hi >> FRCD_HI_BUS_SHIFT) & FRCD_HI_BUS_MASK;
	dev = (fe->hi >> FRCD_HI_DEV_SHIFT) & FRCD_HI_DEV_MASK;
	fun = (fe->hi >> FRCD_HI_FUN_SHIFT) & FRCD_HI_FUN_MASK;
	df  = (fe->hi >> FRCD_HI_FUN_SHIFT) & 0xFF;
	iommu_showcfg(iommu, mksid(bus,dev,fun));
	if (!iommu->ctx[bus]) {
		/* Bus is not initialized */
		mapped = "nobus";
	} else if (!context_entry_is_valid(&iommu->ctx[bus][df])) {
		/* DevFn not initialized */
		mapped = "nodevfn";
	} else if (context_user(&iommu->ctx[bus][df]) != 0xA) {
		/* no bus_space_map */
		mapped = "nomap";
	} else {
		/* bus_space_map */
		mapped = "mapped";
	}
	printf("fri%d: dmar: %.2x:%.2x.%x %s error at %llx fr:%d [%s] iommu:%d [%s]\n",
	    fri, bus, dev, fun,
	    type == 'r' ? "read" : "write",
	    fe->lo,
	    fr, fr <= 13 ? vtd_faults[fr] : "unknown",
	    iommu->id,
	    mapped);
	for (im = bios_memmap; im->type != BIOS_MAP_END; im++) {
		if ((im->type == BIOS_MAP_RES) &&
		    (im->addr <= fe->lo) &&
		    (fe->lo <= im->addr+im->size)) {
			printf("mem in e820.reserved\n");
		}
	}
#ifdef DDB
	if (acpidmar_ddb)
		db_enter();
#endif
}
