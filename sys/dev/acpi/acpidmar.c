/* $OpenBSD: acpidmar.c,v 1.15 2026/03/27 03:56:15 hshoexer Exp $ */
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
#include "amd_iommu.h"

/* We don't want IOMMU to remap MSI */
#define MSI_BASE_ADDRESS	0xFEE00000L
#define MSI_BASE_SIZE		0x00100000L
#define RESERVED_ADDRESS	0x00000000L
#define RESERVED_SIZE		0x00100000L
#define MAX_DEVFN		65536

/* Intel Queued Invalidation queue */
#define IOMMU_QI_ENTRIES	256
#define IOMMU_QI_SIZE	(IOMMU_QI_ENTRIES * sizeof(struct qi_entry))
#define IOMMU_QI_MASK	(IOMMU_QI_SIZE - 1)

/* Range limit before per-domain invalidation */
#define IOMMU_TLB_RANGE_MAX_DESCS_INTEL	128
#define IOMMU_TLB_RANGE_MAX_PAGES_AMD	64

#ifdef IOMMU_DEBUG
int acpidmar_dbg_lvl = 0;
#define DPRINTF(lvl,x...) if (acpidmar_dbg_lvl >= lvl) { printf(x); }
#else
#define DPRINTF(lvl,x...)
#endif

#ifdef DDB
int	acpidmar_ddb = 0;
#endif

int	acpidmar_force_cm = 1;

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

/* Alias mapping */
#define SID_INVALID 0x80000000L
static uint32_t sid_flag[MAX_DEVFN];

struct domain_dev {
	int			sid;
	int			sec;
	int			sub;
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

	struct mutex		ptlck;
	struct mutex		exlck;
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

struct ivhd_devlist {
	int				start_id;
	int				end_id;
	int				cfg;
	TAILQ_ENTRY(ivhd_devlist)	link;
};

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

struct ivmd_entry {
	TAILQ_ENTRY(ivmd_entry) link;
	uint64_t		addr;
	uint64_t		size;
	uint16_t		start_id;
	uint16_t		end_id;
	uint8_t			flags;
};

struct iommu_pic {
	struct pic		pic;
	struct iommu_softc	*iommu;
};

#define IOMMU_FLAGS_CATCHALL		0x1
#define IOMMU_FLAGS_BAD			0x2
#define IOMMU_FLAGS_SUSPEND		0x4
#define IOMMU_FLAGS_COHERENT		0x8

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

	/* Queued Invalidation */
	int			qi_head;
	int			qi_tail;
	paddr_t			qip;
	struct qi_entry		*qi;

	struct domain		*unity;
	TAILQ_HEAD(,domain)	domains;

	/* AMD iommu */
	struct ivhd_dte		*dte;
	void			*cmd_tbl;
	void			*evt_tbl;
	paddr_t			cmd_tblp;
	paddr_t			evt_tblp;

	/* AMD completion wait */
	struct mutex		wait_lock;
	void			*wait_tbl;
	paddr_t			wait_tblp;
	uint32_t		wait_seq;
};

static inline int
iommu_bad(struct iommu_softc *sc)
{
	return (sc->flags & IOMMU_FLAGS_BAD);
}

static inline int
iommu_enabled(struct iommu_softc *sc)
{
	if (sc->dte) {
		return 1;
	}
	return (sc->gcmd & GCMD_TE);
}

struct acpidmar_softc {
	struct device		sc_dev;

	pci_chipset_tag_t	sc_pc;
	bus_space_tag_t		sc_memt;
	int			sc_haw;
	int			sc_flags;
	bus_dma_tag_t		sc_dmat;

	struct ivhd_dte		*sc_hwdte;
	paddr_t			sc_hwdtep;

	TAILQ_HEAD(,iommu_softc)sc_drhds;
	TAILQ_HEAD(,rmrr_softc)	sc_rmrrs;
	TAILQ_HEAD(,atsr_softc)	sc_atsrs;

	TAILQ_HEAD(,ivmd_entry) sc_ivmds;
};

int		acpidmar_activate(struct device *, int);
int		acpidmar_match(struct device *, void *, void *);
void		acpidmar_attach(struct device *, struct device *, void *);
struct domain	*acpidmar_pci_attach(struct acpidmar_softc *, int, int, int);

const struct cfattach acpidmar_ca = {
	sizeof(struct acpidmar_softc), acpidmar_match, acpidmar_attach, NULL,
	acpidmar_activate
};

struct cfdriver acpidmar_cd = {
	NULL, "acpidmar", DV_DULL
};

struct		acpidmar_softc *acpidmar_sc;
int		acpidmar_intr(void *);
int		acpiivhd_intr(void *);

#define DID_UNITY 0x1

void _dumppte(struct pte_entry *, int, vaddr_t);

struct domain *domain_create(struct iommu_softc *, int);
struct domain *domain_lookup(struct acpidmar_softc *, int, int);

void domain_unload_map(struct domain *, bus_dmamap_t);
int domain_load_map(struct domain *, bus_dmamap_t, int, int, const char *);

int (*domain_map_page)(struct domain *, vaddr_t, paddr_t, uint64_t, int);
int domain_map_page_amd(struct domain *, vaddr_t, paddr_t, uint64_t, int);
int domain_map_page_intel(struct domain *, vaddr_t, paddr_t, uint64_t, int);
int domain_map_pthru(struct domain *, paddr_t, paddr_t);

void acpidmar_pci_hook(pci_chipset_tag_t, struct pci_attach_args *);
void acpidmar_parse_devscope(union acpidmar_entry *, int, int,
    struct devlist_head *);
int acpidmar_match_devscope(struct devlist_head *, pci_chipset_tag_t, int);

void acpidmar_init(struct acpidmar_softc *, struct acpi_dmar *);
void acpidmar_drhd(struct acpidmar_softc *, union acpidmar_entry *);
void acpidmar_rmrr(struct acpidmar_softc *, union acpidmar_entry *);
void acpidmar_atsr(struct acpidmar_softc *, union acpidmar_entry *);
void acpiivrs_init(struct acpidmar_softc *, struct acpi_ivrs *);

void *acpidmar_intr_establish(void *, int, int (*)(void *), void *,
    const char *);

void iommu_write_4(struct iommu_softc *, int, uint32_t);
uint32_t iommu_read_4(struct iommu_softc *, int);
void iommu_write_8(struct iommu_softc *, int, uint64_t);
uint64_t iommu_read_8(struct iommu_softc *, int);
void iommu_showfault(struct iommu_softc *, int,
    struct fault_entry *);
void iommu_showcfg(struct iommu_softc *, int);

int iommu_init(struct acpidmar_softc *, struct iommu_softc *,
    struct acpidmar_drhd *);
int iommu_enable_translation(struct iommu_softc *, int);
void iommu_enable_qi(struct iommu_softc *, int);
void iommu_flush_cache(struct iommu_softc *, void *, size_t);
void *iommu_alloc_page(struct iommu_softc *, paddr_t *, int);
void iommu_free_page(void *);
void iommu_flush_write_buffer(struct iommu_softc *);
void iommu_issue_qi(struct iommu_softc *, struct qi_entry *);

void iommu_flush_ctx(struct iommu_softc *, int, int, int, int);
void iommu_flush_ctx_qi(struct iommu_softc *, int, int, int, int);
void iommu_flush_tlb(struct iommu_softc *, int, int);
static void iommu_flush_tlb_reg(struct iommu_softc *, int, int);
void iommu_flush_tlb_qi(struct iommu_softc *, int, int);
void iommu_flush_tlb_page(struct iommu_softc *, int, bus_addr_t);
static void iommu_flush_tlb_segs(struct iommu_softc *, int, bus_dma_segment_t *, int);

/* AMD-Vi invalidation */
static int ivhd_invalidate_page(struct iommu_softc *, int, bus_addr_t);
static void ivhd_completion_wait(struct iommu_softc *);
static void ivhd_invalidate_segs(struct iommu_softc *, int, bus_dma_segment_t *, int);

void iommu_set_rtaddr(struct iommu_softc *, paddr_t);

void *iommu_alloc_hwdte(struct acpidmar_softc *, size_t, paddr_t *);

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
static int dmar_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
static int dmar_dmamem_alloc_range(bus_dma_tag_t, bus_size_t, bus_size_t,
    bus_size_t, bus_dma_segment_t *, int, int *, int, bus_addr_t, bus_addr_t);
static void dmar_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
static int dmar_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
static void dmar_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
static paddr_t	dmar_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int, off_t,
    int, int);

struct dmar_map_cookie {
	int	dm_orig_flags;
};

static inline int
dmar_dmamap_origflags(bus_dmamap_t dmam)
{
	struct dmar_map_cookie *mc = dmam->_dm_cookie;

	if (mc)
		return mc->dm_orig_flags;
	return dmam->_dm_flags;
}

static void dmar_dumpseg(bus_dma_tag_t, int, bus_dma_segment_t *, const char *);
const char *dom_bdf(struct domain *);
void domain_map_check(struct domain *);

struct pte_entry *pte_lvl(struct domain *, struct pte_entry *, vaddr_t,
	int, uint64_t, int, int, int *);
int  ivhd_poll_events(struct iommu_softc *);
void ivhd_showreg(struct iommu_softc *);
void ivhd_showdte(struct iommu_softc *);
void ivhd_showcmd(struct iommu_softc *);

static inline int
debugme(struct domain *dom)
{
	return 0;
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

		if (iommu->dte)
			continue;

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
	struct iommu_softc *iommu;
	int error;

	if (!acpidmar_sc)
		return;
	iommu = dom->iommu;
	domain_map_check(dom);
	error = domain_map_page(dom, addr, addr, PTE_P | PTE_R | PTE_W, 0);
	if (error != 0) {
		printf("%s: ptmap %.16llx failed (%d)\n",
		    dom_bdf(dom), (uint64_t)addr, error);
		return;
	}

	/* Ensure the new translation is visible */
	if (iommu_enabled(iommu)) {
		iommu_flush_write_buffer(iommu);
		iommu_flush_tlb_page(iommu, dom->did, addr);
	}
}

/* Map a range of pages 1:1 */
int
domain_map_pthru(struct domain *dom, paddr_t start, paddr_t end)
{
	struct iommu_softc *iommu = dom->iommu;
	int error = 0;

	domain_map_check(dom);
	while (start < end) {
		error = domain_map_page(dom, start, start, PTE_P | PTE_R | PTE_W, 0);
		if (error)
			break;
		start += VTD_PAGE_SIZE;
	}
	if (error) {
		printf("%s: pthru map failed (%d)\n", dom_bdf(dom), error);
		dom->iommu->flags |= IOMMU_FLAGS_BAD;
		return (error);
	}

	/* Ensure translations are visible */
	if (iommu_enabled(iommu)) {
		iommu_flush_write_buffer(iommu);
		iommu_flush_tlb(iommu, IOTLB_DOMAIN, dom->did);
	}
	return (error);
}

/* Map a single paddr to IOMMU paddr */
int
domain_map_page_intel(struct domain *dom, vaddr_t va, paddr_t pa, uint64_t flags,
    int nowait)
{
	paddr_t paddr;
	struct pte_entry *pte, *npte;
	void *newva;
	uint64_t val;
	int lvl, idx;
	struct iommu_softc *iommu;
	int create;

	iommu = dom->iommu;
	create = ((flags & PTE_P) != 0);

	/* Only handle 4k pages for now */
	npte = dom->pte;
	for (lvl = iommu->agaw - VTD_STRIDE_SIZE; lvl>= VTD_LEVEL0;
	    lvl -= VTD_STRIDE_SIZE) {
		idx = (va >> lvl) & VTD_STRIDE_MASK;
		pte = &npte[idx];

		if (lvl == VTD_LEVEL0) {
			/* Level 1: Page Table */
			pte->val = create ? (pa | flags) : 0;
			iommu_flush_cache(iommu, pte, sizeof(*pte));
			return (0);
		}

		val = pte->val;
		if (val & PTE_P) {
			npte = (void *)PMAP_DIRECT_MAP((val & VTD_PTE_MASK));
			continue;
		}

		if (!create)
			return (0);

		newva = iommu_alloc_page(iommu, &paddr, nowait);
		if (newva == NULL)
			return (ENOMEM);

		/* Avoid concurrent mapping races */
		mtx_enter(&dom->ptlck);
		val = pte->val;
		if (!(val & PTE_P)) {
			pte->val = paddr | PTE_P | PTE_R | PTE_W;
			iommu_flush_cache(iommu, pte, sizeof(*pte));
			val = pte->val;
			newva = NULL; /* owned by the page tables */
		}
		mtx_leave(&dom->ptlck);

		if (newva != NULL)
			iommu_free_page(newva);

		npte = (void *)PMAP_DIRECT_MAP((val & VTD_PTE_MASK));
	}

	return (0);
}

/* Map a single paddr to IOMMU paddr: AMD
 * physical address breakdown into levels:
 * xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx
 *        5.55555555.44444444.43333333,33222222.22211111.1111----.--------
 * mode:
 *  000 = none   shift
 *  001 = 1 [21].12
 *  010 = 2 [30].21
 *  011 = 3 [39].30
 *  100 = 4 [48].39
 *  101 = 5 [57]
 *  110 = 6
 *  111 = reserved
 */
struct pte_entry *
pte_lvl(struct domain *dom, struct pte_entry *pte, vaddr_t va,
	int shift, uint64_t flags, int nowait, int create, int *error)
{
	struct iommu_softc *iommu = dom->iommu;
	void *newva;
	paddr_t paddr;
	uint64_t val;
	int idx;

	idx = (va >> shift) & VTD_STRIDE_MASK;
	val = pte[idx].val;

	if (!(val & PTE_P)) {
		/* Don't allocate lower levels for an unmap */
		if (!create)
			return NULL;

		newva = iommu_alloc_page(iommu, &paddr, nowait);
		if (newva == NULL) {
			if (error)
				*error = ENOMEM;
			return NULL;
		}

		mtx_enter(&dom->ptlck);
		val = pte[idx].val;
		if (!(val & PTE_P)) {
			pte[idx].val = paddr | flags;
			iommu_flush_cache(iommu, &pte[idx], sizeof(pte[idx]));
			val = pte[idx].val;
			newva = NULL; /* owned by the page tables */
		}
		mtx_leave(&dom->ptlck);

		if (newva != NULL)
			iommu_free_page(newva);
	}

	return (void *)PMAP_DIRECT_MAP((val & PTE_PADDR_MASK));
}

int
domain_map_page_amd(struct domain *dom, vaddr_t va, paddr_t pa, uint64_t flags,
    int nowait)
{
	struct pte_entry *pte;
	struct iommu_softc *iommu;
	int idx, error = 0;
	int create;

	iommu = dom->iommu;
	create = ((flags & PTE_P) != 0);

	/* Always assume AMD levels=4                           */
	/*        39        30        21        12              */
	/* ---------|---------|---------|---------|------------ */
	pte = dom->pte;
	pte = pte_lvl(dom, pte, va, 30,
	    PTE_NXTLVL(2) | PTE_IR | PTE_IW | PTE_P,
	    nowait, create, &error);
	if (pte == NULL)
		return (error);
	pte = pte_lvl(dom, pte, va, 21,
	    PTE_NXTLVL(1) | PTE_IR | PTE_IW | PTE_P,
	    nowait, create, &error);
	if (pte == NULL)
		return (error);

	if (create)
		flags = PTE_P | PTE_R | PTE_W | PTE_IW | PTE_IR | PTE_NXTLVL(0);
	else
		flags = 0;

	/* Level 1: Page Table */
	idx = (va >> 12) & 0x1FF;
	pte[idx].val = create ? (pa | flags) : 0;
	iommu_flush_cache(iommu, &pte[idx], sizeof(pte[idx]));

	return (0);
}

static void
dmar_dumpseg(bus_dma_tag_t tag, int nseg, bus_dma_segment_t *segs,
    const char *lbl)
{
	struct domain *dom = tag->_cookie;
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
	struct iommu_softc	*iommu;
	paddr_t			base, end, idx;
	psize_t			alen;
	int			i;

	iommu = dom->iommu;
	if (iommu_bad(iommu)) {
		printf("unload map no iommu\n");
		return;
	}
	if (dmam->dm_nsegs == 0)
		return;

	/* First pass: clear PTEs */
	for (i = 0; i < dmam->dm_nsegs; i++) {
		seg = &dmam->dm_segs[i];

		base = trunc_page(seg->ds_addr);
		end = roundup(seg->ds_addr + seg->ds_len, VTD_PAGE_SIZE);
		alen = end - base;

		if (debugme(dom)) {
			printf("  va:%.16llx len:%x\n",
			    (uint64_t)base, (uint32_t)alen);
		}

		for (idx = 0; idx < alen; idx += VTD_PAGE_SIZE)
			domain_map_page(dom, base + idx, 0, 0, 1);
	}

	/* Flush translations */
	iommu_flush_write_buffer(iommu);
	iommu_flush_tlb_segs(iommu, dom->did, dmam->dm_segs, dmam->dm_nsegs);

	/* Second pass: free IOVA space */
	for (i = 0; i < dmam->dm_nsegs; i++) {
		seg = &dmam->dm_segs[i];

		base = trunc_page(seg->ds_addr);
		end = roundup(seg->ds_addr + seg->ds_len, VTD_PAGE_SIZE);
		alen = end - base;

		if (dom->flag & DOM_NOMAP) {
			printf("%s: nomap %.16llx\n",
			    dom_bdf(dom), (uint64_t)base);
			continue;
		}

		mtx_enter(&dom->exlck);
		if (extent_free(dom->iovamap, base, alen, EX_NOWAIT)) {
			mtx_leave(&dom->exlck);
			printf("%s: extent_free %.16llx %x failed\n",
			    dom->exname, (uint64_t)base, (uint32_t)alen);
			continue;
		}
		mtx_leave(&dom->exlck);
	}
}

/* map.segs[x].ds_addr is modified to IOMMU virtual PA */
int
domain_load_map(struct domain *dom, bus_dmamap_t map, int flags, int pteflag,
    const char *fn)
{
	bus_dma_segment_t	*seg;
	bus_size_t		align, boundary;
	struct iommu_softc	*iommu;
	paddr_t			base, end, idx;
	psize_t			alen;
	u_long			res, sgstart, sgend;
	int			i, mflags, mapped_nsegs, error;
	int			pt_nowait;

	iommu = dom->iommu;
	if (iommu_bad(iommu))
		return (ENXIO);
	if (!iommu_enabled(iommu)) {
		/* Lazy enable translation when required */
		error = iommu_enable_translation(iommu, 1);
		if (error)
			return (error);
	}

	mflags = dmar_dmamap_origflags(map);
	pt_nowait = (flags & BUS_DMA_NOWAIT);
	mapped_nsegs = 0;
	error = 0;

	/*
	 * A boundary presented to bus_dmamem_alloc() takes precedence
	 * over boundary in the map.
	 */
	if ((boundary = (map->dm_segs[0]._ds_boundary)) == 0)
		boundary = map->_dm_boundary;
	align = MAX(map->dm_segs[0]._ds_align, VTD_PAGE_SIZE);

	for (i = 0; i < map->dm_nsegs; i++) {
		seg = &map->dm_segs[i];

		base = trunc_page(seg->ds_addr);
		end = roundup(seg->ds_addr + seg->ds_len, VTD_PAGE_SIZE);
		alen = end - base;
		res = base;

		if ((dom->flag & DOM_NOMAP) == 0) {
			/* Allocate DMA Virtual Address */
			sgstart = dom->iovamap->ex_start;
			sgend = dom->iovamap->ex_end;

			if (mflags & BUS_DMA_24BIT) {
				if (sgend > 0x00ffffffUL)
					sgend = 0x00ffffffUL;
			} else if ((mflags & BUS_DMA_64BIT) == 0) {
				if (sgend > 0xffffffffUL)
					sgend = 0xffffffffUL;
			}

			mtx_enter(&dom->exlck);
			if (extent_alloc_subregion(dom->iovamap, sgstart,
			    sgend, alen, align, 0, boundary,
			    EX_NOWAIT, &res)) {
				mtx_leave(&dom->exlck);
				error = ENOMEM;
				goto fail;
			}
			if (res == (u_long)-1) {
				mtx_leave(&dom->exlck);
				error = EIO;
				goto fail;
			}
			mtx_leave(&dom->exlck);

			/* Reassign DMA address */
			seg->ds_addr = res | (seg->ds_addr & VTD_PAGE_MASK);
		}

		if (debugme(dom)) {
			printf("  LOADMAP: %.16llx %x => %.16llx\n",
			    (uint64_t)seg->ds_addr, (uint32_t)seg->ds_len,
			    (uint64_t)res);
		}

		mapped_nsegs = i + 1;

		for (idx = 0; idx < alen; idx += VTD_PAGE_SIZE) {
			error = domain_map_page(dom, res + idx, base + idx,
			    PTE_P | pteflag, pt_nowait);
			if (error)
				goto fail;
		}
	}

	iommu_flush_write_buffer(iommu);
	iommu_flush_tlb_segs(iommu, dom->did, map->dm_segs, map->dm_nsegs);
	return (0);

fail:
	if (mapped_nsegs > 0) {
		int save_nsegs = map->dm_nsegs;

		map->dm_nsegs = mapped_nsegs;
		domain_unload_map(dom, map);
		map->dm_nsegs = save_nsegs;
	}
	return (error);
}

const char *
dom_bdf(struct domain *dom)
{
	struct domain_dev *dd;
	static char		mmm[48];

	dd = TAILQ_FIRST(&dom->devices);
	snprintf(mmm, sizeof(mmm), "%s iommu:%d did:%.4x%s",
	    dmar_bdf(dd->sid), dom->iommu->id, dom->did,
	    dom->did == DID_UNITY ? " [unity]" : "");
	return (mmm);
}

/* Bus DMA Map functions */
static int
dmar_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct dmar_map_cookie	*mc;
	bus_dmamap_t			dmam;
	int				rc;

	rc = _bus_dmamap_create(tag, size, nsegments, maxsegsz, boundary,
	    flags | BUS_DMA_64BIT, dmamp);
	if (rc)
		return (rc);

	dmam = *dmamp;
	mc = malloc(sizeof(*mc), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	if (mc == NULL) {
		_bus_dmamap_destroy(tag, dmam);
		*dmamp = NULL;
		return (ENOMEM);
	}
	mc->dm_orig_flags = flags & ~(BUS_DMA_WAITOK | BUS_DMA_NOWAIT);
	dmam->_dm_cookie = mc;

	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	return (0);
}

static void
dmar_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	struct dmar_map_cookie	*mc = dmam->_dm_cookie;

	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	if (mc != NULL) {
		free(mc, M_DEVBUF, sizeof(*mc));
		dmam->_dm_cookie = NULL;
	}
	_bus_dmamap_destroy(tag, dmam);
}

static int
dmar_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct domain *dom = tag->_cookie;
	int		rc;

	rc = _bus_dmamap_load(tag, dmam, buf, buflen, p, flags);
	if (!rc) {
		/* Clear stale allocation constraints */
		if (dmam->dm_nsegs > 0) {
			dmam->dm_segs[0]._ds_align = 0;
			dmam->dm_segs[0]._ds_boundary = 0;
		}

		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		rc = domain_load_map(dom, dmam, flags, PTE_R|PTE_W,
		    __FUNCTION__);
		if (rc) {
			_bus_dmamap_unload(tag, dmam);
			return (rc);
		}
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
		/* Clear stale allocation constraints */
		if (dmam->dm_nsegs > 0) {
			dmam->dm_segs[0]._ds_align = 0;
			dmam->dm_segs[0]._ds_boundary = 0;
		}

		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		rc = domain_load_map(dom, dmam, flags, PTE_R|PTE_W,
		    __FUNCTION__);
		if (rc) {
			_bus_dmamap_unload(tag, dmam);
			return (rc);
		}
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
		/* Clear stale allocation constraints */
		if (dmam->dm_nsegs > 0) {
			dmam->dm_segs[0]._ds_align = 0;
			dmam->dm_segs[0]._ds_boundary = 0;
		}

		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		rc = domain_load_map(dom, dmam, flags, PTE_R|PTE_W,
		    __FUNCTION__);
		if (rc) {
			_bus_dmamap_unload(tag, dmam);
			return (rc);
		}
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
		/* Preserve _ds_align/_ds_boundary allocation constraints */
		if (nsegs > 0 && dmam->dm_nsegs > 0) {
			dmam->dm_segs[0]._ds_align = segs[0]._ds_align;
			dmam->dm_segs[0]._ds_boundary = segs[0]._ds_boundary;
		}

		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
		rc = domain_load_map(dom, dmam, flags, PTE_R|PTE_W,
		    __FUNCTION__);
		if (rc) {
			_bus_dmamap_unload(tag, dmam);
			return (rc);
		}
		dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs,
		    __FUNCTION__);
	}
	return (rc);
}

static void
dmar_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t dmam)
{
	struct domain *dom = tag->_cookie;

	dmar_dumpseg(tag, dmam->dm_nsegs, dmam->dm_segs, __FUNCTION__);
	domain_unload_map(dom, dmam);
	_bus_dmamap_unload(tag, dmam);
}

static int
dmar_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	int rc;

	/* Enforce device DMA mask in the IOVA allocator (domain_load_map) */
	rc = _bus_dmamem_alloc(tag, size, alignment, boundary, segs, nsegs,
	    rsegs, flags | BUS_DMA_64BIT);
	if (!rc) {
		dmar_dumpseg(tag, *rsegs, segs, __FUNCTION__);
	}
	return (rc);
}

static int
dmar_dmamem_alloc_range(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags, bus_addr_t low, bus_addr_t high)
{
	int rc;

	/* Honor physical address range */
	rc = _bus_dmamem_alloc_range(tag, size, alignment, boundary, segs, nsegs,
	    rsegs, flags, low, high);
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

/* Intel: Set Context Root Address */
void
iommu_set_rtaddr(struct iommu_softc *iommu, paddr_t paddr)
{
	int i, sts;

	mtx_enter(&iommu->reg_lock);
	iommu_write_8(iommu, DMAR_RTADDR_REG, paddr);
	iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd | GCMD_SRTP);
	for (i = 0; i < 5; i++) {
		sts = iommu_read_4(iommu, DMAR_GSTS_REG);
		if (sts & GSTS_RTPS)
			break;
		delay(10000);
	}
	mtx_leave(&iommu->reg_lock);

	if (i == 5) {
		printf("iommu%d: set_rtaddr timeout\n", iommu->id);
	}
}

/* Allocate contiguous memory (1Mb) for the Device Table Entries */
void *
iommu_alloc_hwdte(struct acpidmar_softc *sc, size_t size, paddr_t *paddr)
{
	caddr_t vaddr;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	bus_dma_tag_t dmat = sc->sc_dmat;
	int rc, nsegs;

	rc = _bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &map);
	if (rc != 0) {
		printf("hwdte_create fails\n");
		return NULL;
	}
	rc = _bus_dmamem_alloc(dmat, size, 4, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (rc != 0) {
		printf("hwdte alloc fails\n");
		return NULL;
	}
	rc = _bus_dmamem_map(dmat, &seg, 1, size, &vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (rc != 0) {
		printf("hwdte map fails\n");
		return NULL;
	}
	rc = _bus_dmamap_load_raw(dmat, map, &seg, 1, size, BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("hwdte load raw fails\n");
		return NULL;
	}
	*paddr = map->dm_segs[0].ds_addr;
	return vaddr;
}

/* COMMON: Allocate a new memory page */
void *
iommu_alloc_page(struct iommu_softc *iommu, paddr_t *paddr, int nowait)
{
	void *va;
	const struct kmem_dyn_mode *kd;

	*paddr = 0;
	kd = nowait ? &kd_trylock : &kd_waitok;
	va = km_alloc(VTD_PAGE_SIZE, &kv_page, &kp_zero, kd);
	if (va == NULL)
		return (NULL);
	pmap_extract(pmap_kernel(), (vaddr_t)va, paddr);

	iommu_flush_cache(iommu, va, VTD_PAGE_SIZE);

	return (va);
}

void
iommu_free_page(void *va)
{
	if (va == NULL)
		return;
	km_free(va, VTD_PAGE_SIZE, &kv_page, &kp_zero);
}

/* Intel: Issue command via queued invalidation */
void
iommu_issue_qi(struct iommu_softc *iommu, struct qi_entry *qi)
{
	uint64_t h, t;
	int tail, next;
	int n;

	if (iommu->qi == NULL)
		return;
	if ((iommu->gcmd & GCMD_QIE) == 0)
		return;

	mtx_enter(&iommu->reg_lock);

	tail = iommu->qi_tail;
	next = (tail + 1) % IOMMU_QI_ENTRIES;

	/* Avoid filling the queue: wait if next == head */
	h = iommu_read_8(iommu, DMAR_IQH_REG) & IOMMU_QI_MASK;
	if (((uint64_t)next * sizeof(struct qi_entry)) == h) {
		for (n = 0; n < 1000; n++) {
			delay(10);
			h = iommu_read_8(iommu, DMAR_IQH_REG) & IOMMU_QI_MASK;
			if (((uint64_t)next * sizeof(struct qi_entry)) != h)
				break;
		}
	}

	if (((uint64_t)next * sizeof(struct qi_entry)) == h) {
		printf("iommu%d: QI queue full\n", iommu->id);
		mtx_leave(&iommu->reg_lock);
		return;
	}

	iommu->qi[tail] = *qi;
	iommu_flush_cache(iommu, &iommu->qi[tail], sizeof(*qi));

	iommu->qi_tail = next;
	t = (uint64_t)next * sizeof(struct qi_entry);
	iommu_write_8(iommu, DMAR_IQT_REG, t);

	/* Wait for the descriptor to be processed */
	for (n = 0; n < 1000; n++) {
		h = iommu_read_8(iommu, DMAR_IQH_REG) & IOMMU_QI_MASK;
		if (h == t)
			break;
		delay(10);
	}
	if (h != t)
		printf("iommu%d: QI timeout (h=%llx t=%llx)\n",
		    iommu->id, (unsigned long long)h,
		    (unsigned long long)t);

	mtx_leave(&iommu->reg_lock);
}

/* Intel: Flush TLB entries, Queued Invalidation mode */
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
		/*
		 * Page-selective invalidation requires an address; use
		 * iommu_flush_tlb_page()
		 */
		qi.lo = QI_IOTLB | QI_IOTLB_IG_DOMAIN | QI_IOTLB_DID(did);
		break;
	}
	if (iommu->cap & CAP_DRD)
		qi.lo |= QI_IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		qi.lo |= QI_IOTLB_DW;
	iommu_issue_qi(iommu, &qi);
}

/* Intel: Flush Context entries, Queued Invalidation mode */
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

/* Intel: Flush write buffers */
void
iommu_flush_write_buffer(struct iommu_softc *iommu)
{
	int i, sts;

	if (iommu->dte)
		return;
	if (!(iommu->cap & CAP_RWBF))
		return;

	DPRINTF(1,"writebuf\n");

	mtx_enter(&iommu->reg_lock);

	iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd | GCMD_WBF);
	for (i = 0; i < 5; i++) {
		sts = iommu_read_4(iommu, DMAR_GSTS_REG);
		if (sts & GSTS_WBFS)
			break;
		delay(10000);
	}

	mtx_leave(&iommu->reg_lock);

	if (i == 5) {
		printf("write buffer flush fails\n");
	}
}

void
iommu_flush_cache(struct iommu_softc *iommu, void *addr, size_t size)
{
	if (iommu->dte) {
		if (iommu->flags & IOMMU_FLAGS_COHERENT)
			return;
		pmap_flush_cache((vaddr_t)addr, size);
		return;
	}
	if (!(iommu->ecap & ECAP_C))
		pmap_flush_cache((vaddr_t)addr, size);
}

static void
iommu_flush_tlb_reg(struct iommu_softc *iommu, int mode, int did)
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
		/*
		 * Page-selective invalidation requires an address; use
		 * iommu_flush_tlb_page().
		 */
		val |= IIG_DOMAIN | IOTLB_DID(did);
		break;
	}

	/* Check for Read/Write Drain */
	if (iommu->cap & CAP_DRD)
		val |= IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		val |= IOTLB_DW;

	mtx_enter(&iommu->reg_lock);

	iommu_write_8(iommu, DMAR_IOTLB_REG(iommu), val);
	for (n = 0; n < 5; n++) {
		val = iommu_read_8(iommu, DMAR_IOTLB_REG(iommu));
		if ((val & IOTLB_IVT) == 0)
			break;
		delay(10000);
	}
	if (val & IOTLB_IVT)
		printf("iommu%d: IOTLB invalidation timeout\n", iommu->id);

	mtx_leave(&iommu->reg_lock);
}


/* Flush IOMMU TLB entries globally or per-domain */
void
iommu_flush_tlb(struct iommu_softc *iommu, int mode, int did)
{
	/* Call AMD */
	if (iommu->dte) {
		ivhd_invalidate_domain(iommu, did);
		return;
	}
	if ((iommu->gcmd & GCMD_QIE) && iommu->qi != NULL) {
		iommu_flush_tlb_qi(iommu, mode, did);
		return;
	}

	iommu_flush_tlb_reg(iommu, mode, did);
}


/*
 * Flush IOMMU TLB entries for a single IOVA page
 */
void
iommu_flush_tlb_page(struct iommu_softc *iommu, int did, bus_addr_t iova)
{
	struct qi_entry qi;
	uint64_t val, iva;
	int n;

	iova &= ~(bus_addr_t)VTD_PAGE_MASK;
	if (iommu->dte) {
		/* AMD: page-selective invalidation via INVALIDATE_IOMMU_PAGES */
		if (ivhd_invalidate_page(iommu, did, iova) < 0)
			ivhd_invalidate_domain(iommu, did);
		return;
	}

	/* Page-selective invalidation requires PSI capability */
	if ((iommu->cap & CAP_PSI) == 0) {
		iommu_flush_tlb(iommu, IOTLB_DOMAIN, did);
		return;
	}

	if ((iommu->gcmd & GCMD_QIE) && iommu->qi != NULL) {
		qi.lo = QI_IOTLB | QI_IOTLB_IG_PAGE | QI_IOTLB_DID(did);
		if (iommu->cap & CAP_DRD)
			qi.lo |= QI_IOTLB_DR;
		if (iommu->cap & CAP_DWD)
			qi.lo |= QI_IOTLB_DW;

		/*
		 * Invalidate exactly one 4K page. Low bits are reserved for the
		 * address mask field. Keeping them zero requests a single-page
		 * invalidation.
		 */
		qi.hi = (uint64_t)iova;
		iommu_issue_qi(iommu, &qi);
		return;
	}

	iva = (uint64_t)iova;

	val = IOTLB_IVT | IIG_PAGE | IOTLB_DID(did);
	if (iommu->cap & CAP_DRD)
		val |= IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		val |= IOTLB_DW;

	mtx_enter(&iommu->reg_lock);

	/* Program the invalidate address register (AM=0 => one page) */
	iommu_write_8(iommu, DMAR_IVA_REG(iommu), iva);

	iommu_write_8(iommu, DMAR_IOTLB_REG(iommu), val);
	for (n = 0; n < 5; n++) {
		val = iommu_read_8(iommu, DMAR_IOTLB_REG(iommu));
		if ((val & IOTLB_IVT) == 0)
			break;
		delay(10000);
	}
	if (val & IOTLB_IVT)
		printf("iommu%d: IOTLB page invalidation timeout\n", iommu->id);

	mtx_leave(&iommu->reg_lock);
}

/*
 * Intel: Flush IOTLB entries for a set of IOVA ranges
 */
static inline int
iommu_intel_pick_am(bus_addr_t addr, bus_addr_t end, int max_am)
{
	int		am;
	uint64_t	bytes;

	/* AM=0 invalidates exactly one 4KB page */
	if (max_am <= 0)
		return (0);

	for (am = max_am; am > 0; am--) {
		bytes = (uint64_t)VTD_PAGE_SIZE << am;
		if ((addr & (bytes - 1)) != 0)
			continue;
		if ((uint64_t)(end - addr) < bytes)
			continue;
		return (am);
	}
	return (0);
}

static void
iommu_flush_tlb_segs(struct iommu_softc *iommu, int did,
    bus_dma_segment_t *segs, int nsegs)
{
	struct qi_entry	qi;
	bus_addr_t	start, end, addr;
	u_long		npages, ndescs;
	int		max_am, am;
	uint64_t	h, t;
	uint64_t	cmd, iva, sts;
	int		tail, head, nfree;
	int		i, n;

	if (segs == NULL || nsegs <= 0)
		return;

	npages = 0;
	for (i = 0; i < nsegs; i++) {
		if (segs[i].ds_len == 0)
			continue;
		start = segs[i].ds_addr & ~(bus_addr_t)VTD_PAGE_MASK;
		end = roundup(segs[i].ds_addr + segs[i].ds_len, VTD_PAGE_SIZE);
		if (end > start)
			npages += (end - start) / VTD_PAGE_SIZE;
	}
	if (npages == 0)
		return;

	if (iommu->dte) {
		if (npages > IOMMU_TLB_RANGE_MAX_PAGES_AMD) {
			(void)ivhd_invalidate_domain(iommu, did);
			return;
		}
		ivhd_invalidate_segs(iommu, did, segs, nsegs);
		return;
	}

	/* No page-selective invalidation support */
	if ((iommu->cap & CAP_PSI) == 0) {
		iommu_flush_tlb(iommu, IOTLB_DOMAIN, did);
		return;
	}

	max_am = (int)cap_mamv(iommu->cap);
	if (max_am > 51)
		max_am = 51;

	/*
	 * Estimate invalidation descriptor pressure using AM coalescing.
	 *
	 * If this range still requires too many descriptors, fall back.
	 */
	ndescs = 0;
	for (i = 0; i < nsegs; i++) {
		if (segs[i].ds_len == 0)
			continue;
		start = segs[i].ds_addr & ~(bus_addr_t)VTD_PAGE_MASK;
		end = roundup(segs[i].ds_addr + segs[i].ds_len, VTD_PAGE_SIZE);
		for (addr = start; addr < end; ) {
			am = iommu_intel_pick_am(addr, end, max_am);
			ndescs++;
			addr += (bus_addr_t)((uint64_t)VTD_PAGE_SIZE << am);
		}
	}

	if (ndescs > IOMMU_TLB_RANGE_MAX_DESCS_INTEL) {
		iommu_flush_tlb(iommu, IOTLB_DOMAIN, did);
		return;
	}

	if ((iommu->gcmd & GCMD_QIE) && iommu->qi != NULL) {
		qi.lo = QI_IOTLB | QI_IOTLB_IG_PAGE | QI_IOTLB_DID(did);
		if (iommu->cap & CAP_DRD)
			qi.lo |= QI_IOTLB_DR;
		if (iommu->cap & CAP_DWD)
			qi.lo |= QI_IOTLB_DW;

		/* Batch queued invalidation descriptors */
		mtx_enter(&iommu->reg_lock);

		for (n = 0; ; n++) {
			h = iommu_read_8(iommu, DMAR_IQH_REG) & IOMMU_QI_MASK;
			head = (int)(h / sizeof(struct qi_entry));
			tail = iommu->qi_tail;
			if (tail >= head)
				nfree = IOMMU_QI_ENTRIES - (tail - head) - 1;
			else
				nfree = head - tail - 1;
			if (nfree >= (int)ndescs)
				break;

			mtx_leave(&iommu->reg_lock);
			if (n >= 1000) {
				printf("iommu%d: QI queue full (range flush)\n",
				    iommu->id);
				/* Bypass QI to avoid recursion on a saturated ring */
				iommu_flush_tlb_reg(iommu, IOTLB_DOMAIN, did);
				return;
			}
			delay(10);
			mtx_enter(&iommu->reg_lock);
		}

		for (i = 0; i < nsegs; i++) {
			if (segs[i].ds_len == 0)
				continue;
			start = segs[i].ds_addr & ~(bus_addr_t)VTD_PAGE_MASK;
			end = roundup(segs[i].ds_addr + segs[i].ds_len, VTD_PAGE_SIZE);

			for (addr = start; addr < end; ) {
				am = iommu_intel_pick_am(addr, end, max_am);

				/*
				 * The low bits of the address field hold the Address Mask (AM)
				 * value. AM=0 invalidates one 4KB page. AM>0 invalidates a
				 * power-of-two number of pages for a size-aligned range.
				 */
				qi.hi = (uint64_t)addr | (uint64_t)am;
				iommu->qi[tail] = qi;
				iommu_flush_cache(iommu, &iommu->qi[tail], sizeof(qi));
				tail = (tail + 1) % IOMMU_QI_ENTRIES;

				addr += (bus_addr_t)((uint64_t)VTD_PAGE_SIZE << am);
			}
		}

		iommu->qi_tail = tail;
		t = (uint64_t)tail * sizeof(struct qi_entry);
		iommu_write_8(iommu, DMAR_IQT_REG, t);

		/* Wait for all queued invalidations to be processed */
		for (n = 0; n < 1000; n++) {
			h = iommu_read_8(iommu, DMAR_IQH_REG) & IOMMU_QI_MASK;
			if (h == t)
				break;
			delay(10);
		}
		if (h != t) {
			printf("iommu%d: QI timeout (range h=%llx t=%llx)\n",
			    iommu->id, (unsigned long long)h,
			    (unsigned long long)t);
		}

		mtx_leave(&iommu->reg_lock);
		return;
	}

	/* Register-based PSI */
	cmd = IOTLB_IVT | IIG_PAGE | IOTLB_DID(did);
	if (iommu->cap & CAP_DRD)
		cmd |= IOTLB_DR;
	if (iommu->cap & CAP_DWD)
		cmd |= IOTLB_DW;

	mtx_enter(&iommu->reg_lock);

	for (i = 0; i < nsegs; i++) {
		if (segs[i].ds_len == 0)
			continue;
		start = segs[i].ds_addr & ~(bus_addr_t)VTD_PAGE_MASK;
		end = roundup(segs[i].ds_addr + segs[i].ds_len, VTD_PAGE_SIZE);

		for (addr = start; addr < end; ) {
			am = iommu_intel_pick_am(addr, end, max_am);
			iva = (uint64_t)addr | (uint64_t)am;

			iommu_write_8(iommu, DMAR_IVA_REG(iommu), iva);
			iommu_write_8(iommu, DMAR_IOTLB_REG(iommu), cmd);
			for (n = 0; n < 5; n++) {
				sts = iommu_read_8(iommu, DMAR_IOTLB_REG(iommu));
				if ((sts & IOTLB_IVT) == 0)
					break;
				delay(10000);
			}
			if (sts & IOTLB_IVT)
				printf("iommu%d: IOTLB page invalidation timeout\n",
				    iommu->id);

			addr += (bus_addr_t)((uint64_t)VTD_PAGE_SIZE << am);
		}
	}

	mtx_leave(&iommu->reg_lock);
}

/*
 * Intel: Flush IOMMU settings
 * Flushes can occur globally, per domain, or per device
 */
void
iommu_flush_ctx(struct iommu_softc *iommu, int mode, int did, int sid, int fm)
{
	uint64_t	val;
	int		n;

	if (iommu->dte)
		return;
	if ((iommu->gcmd & GCMD_QIE) && iommu->qi != NULL) {
		iommu_flush_ctx_qi(iommu, mode, did, sid, fm);
		return;
	}

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

	iommu_write_8(iommu, DMAR_CCMD_REG, val);
	for (n = 0; n < 5; n++) {
		val = iommu_read_8(iommu, DMAR_CCMD_REG);
		if ((val & CCMD_ICC) == 0)
			break;
		delay(10000);
	}
	if (val & CCMD_ICC)
		printf("iommu%d: CCMD invalidation timeout\n", iommu->id);

	mtx_leave(&iommu->reg_lock);
}

/* Intel: Enable Queued Invalidation */
void
iommu_enable_qi(struct iommu_softc *iommu, int enable)
{
	int	n;
	int	sts;

	if (iommu->dte)
		return;
	if (!(iommu->ecap & ECAP_QI))
		return;

	if (enable) {
		if (iommu->qi == NULL) {
			printf("iommu%d: QI requested but no queue\n",
			    iommu->id);
			return;
		}

		iommu->gcmd |= GCMD_QIE;

		mtx_enter(&iommu->reg_lock);

		iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
		for (n = 0; n < 100; n++) {
			sts = iommu_read_4(iommu, DMAR_GSTS_REG);
			if (sts & GSTS_QIES)
				break;
			delay(1000);
		}

		mtx_leave(&iommu->reg_lock);

		if (!(sts & GSTS_QIES)) {
			printf("iommu%d: enable QI timeout\n", iommu->id);
			/* Fall back to register invalidation */
			iommu->gcmd &= ~GCMD_QIE;
			mtx_enter(&iommu->reg_lock);
			iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
			mtx_leave(&iommu->reg_lock);
		}
	} else {
		iommu->gcmd &= ~GCMD_QIE;

		mtx_enter(&iommu->reg_lock);

		iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
		for (n = 0; n < 100; n++) {
			sts = iommu_read_4(iommu, DMAR_GSTS_REG);
			if ((sts & GSTS_QIES) == 0)
				break;
			delay(1000);
		}

		mtx_leave(&iommu->reg_lock);

		if (sts & GSTS_QIES)
			printf("iommu%d: disable QI timeout\n", iommu->id);
	}
}

/* Intel: Enable IOMMU translation */
int
iommu_enable_translation(struct iommu_softc *iommu, int enable)
{
	uint32_t	sts;
	int		n = 0;

	if (iommu->dte)
		return (0);
	if (enable) {
		DPRINTF(1, "iommu%d: enable translation\n", iommu->id);
#ifdef IOMMU_DEBUG
		if (acpidmar_dbg_lvl >= 2)
			iommu_showcfg(iommu, -1);
#endif

		iommu->gcmd |= GCMD_TE;

		/* Enable translation */
		mtx_enter(&iommu->reg_lock);
		iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
		do {
			sts = iommu_read_4(iommu, DMAR_GSTS_REG);
			delay(10000);
		} while (n++ < 5 && !(sts & GSTS_TES));
		mtx_leave(&iommu->reg_lock);
		DPRINTF(2, "iommu%d: translation enable loops=%d\n", iommu->id, n);

		if (n >= 5) {
			printf("error.. unable to initialize iommu %d\n",
			    iommu->id);
			iommu->flags |= IOMMU_FLAGS_BAD;

			/* Disable IOMMU */
			iommu->gcmd &= ~GCMD_TE;
			mtx_enter(&iommu->reg_lock);
			iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
			mtx_leave(&iommu->reg_lock);

			return (EIO);
		}

		iommu_flush_ctx(iommu, CTX_GLOBAL, 0, 0, 0);
		iommu_flush_tlb(iommu, IOTLB_GLOBAL, 0);
	} else {
		iommu->gcmd &= ~GCMD_TE;

		mtx_enter(&iommu->reg_lock);

		iommu_write_4(iommu, DMAR_GCMD_REG, iommu->gcmd);
		do {
			sts = iommu_read_4(iommu, DMAR_GSTS_REG);
			delay(10000);
		} while (n++ < 5 && (sts & GSTS_TES));
		mtx_leave(&iommu->reg_lock);
		DPRINTF(2, "iommu%d: translation disable loops=%d\n", iommu->id, n);
	}

	return (0);
}

/* Intel: Initialize IOMMU */
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

	iommu->cap = iommu_read_8(iommu, DMAR_CAP_REG);
	iommu->ecap = iommu_read_8(iommu, DMAR_ECAP_REG);
	iommu->ndoms = cap_nd(iommu->cap);

	/* Print Capabilities & Extended Capabilities */
	DPRINTF(0, "  caps: %s%s%s%s%s%s%s%s%s%s%s\n",
	    iommu->cap & CAP_AFL ? "afl " : "",		/* adv fault */
	    iommu->cap & CAP_RWBF ? "rwbf " : "",	/* write-buffer flush */
	    iommu->cap & CAP_PLMR ? "plmr " : "",	/* protected lo region */
	    iommu->cap & CAP_PHMR ? "phmr " : "",	/* protected hi region */
	    iommu->cap & CAP_CM ? "cm " : "",		/* caching mode */
	    iommu->cap & CAP_ZLR ? "zlr " : "",		/* zero-length read */
	    iommu->cap & CAP_PSI ? "psi " : "",		/* page invalidate */
	    iommu->cap & CAP_DWD ? "dwd " : "",		/* write drain */
	    iommu->cap & CAP_DRD ? "drd " : "",		/* read drain */
	    iommu->cap & CAP_FL1GP ? "Gb " : "",	/* 1Gb pages */
	    iommu->cap & CAP_PI ? "pi " : "");		/* posted interrupts */
	DPRINTF(0, "  ecap: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
	    iommu->ecap & ECAP_C ? "c " : "",		/* coherent */
	    iommu->ecap & ECAP_QI ? "qi " : "",		/* queued invalidate */
	    iommu->ecap & ECAP_DT ? "dt " : "",		/* device iotlb */
	    iommu->ecap & ECAP_IR ? "ir " : "",		/* intr remap */
	    iommu->ecap & ECAP_EIM ? "eim " : "",	/* x2apic */
	    iommu->ecap & ECAP_PT ? "pt " : "",		/* passthrough */
	    iommu->ecap & ECAP_SC ? "sc " : "",		/* snoop control */
	    iommu->ecap & ECAP_ECS ? "ecs " : "",	/* extended context */
	    iommu->ecap & ECAP_MTS ? "mts " : "",	/* memory type */
	    iommu->ecap & ECAP_NEST ? "nest " : "",	/* nested translations */
	    iommu->ecap & ECAP_DIS ? "dis " : "",	/* deferred invalidation */
	    iommu->ecap & ECAP_PASID ? "pas " : "",	/* pasid */
	    iommu->ecap & ECAP_PRS ? "prs " : "",	/* page request */
	    iommu->ecap & ECAP_ERS ? "ers " : "",	/* execute request */
	    iommu->ecap & ECAP_SRS ? "srs " : "",	/* supervisor request */
	    iommu->ecap & ECAP_NWFS ? "nwfs " : "",	/* no write flag */
	    iommu->ecap & ECAP_EAFS ? "eafs " : "");	/* extended accessed flag */

	mtx_init(&iommu->reg_lock, IPL_HIGH);

	/* Clear Interrupt Masking */
	iommu_write_4(iommu, DMAR_FSTS_REG, FSTS_PFO | FSTS_PPF);

	iommu->intr = acpidmar_intr_establish(iommu, IPL_HIGH,
	    acpidmar_intr, iommu, "dmarintr");

	/* Enable interrupts */
	sts = iommu_read_4(iommu, DMAR_FECTL_REG);
	iommu_write_4(iommu, DMAR_FECTL_REG, sts & ~FECTL_IM);

	/* Allocate root pointer */
	iommu->root = iommu_alloc_page(iommu, &paddr, 0);
	if (iommu->root == NULL) {
		printf("iommu%d: can't allocate root pointer\n", iommu->id);
		iommu->flags |= IOMMU_FLAGS_BAD;
		return (-1);
	}
	DPRINTF(0, "Allocated root pointer: pa:%.16llx va:%p\n",
	    (uint64_t)paddr, iommu->root);
	iommu->rtaddr = paddr;
	iommu_flush_write_buffer(iommu);
	iommu_set_rtaddr(iommu, paddr);

	if (iommu->ecap & ECAP_QI) {
		/* Queued Invalidation support */
		iommu->qi = iommu_alloc_page(iommu, &iommu->qip, 0);
		if (iommu->qi == NULL) {
			printf("iommu%d: can't allocate QI queue\n",
			    iommu->id);
		} else {
			iommu->qi_head = 0;
			iommu->qi_tail = 0;
			iommu_write_8(iommu, DMAR_IQT_REG, 0);
			iommu_write_8(iommu, DMAR_IQA_REG,
			    (iommu->qip & ~0xfffULL) | IQA_QS_256);
			iommu_flush_write_buffer(iommu);
			iommu_enable_qi(iommu, 1);
		}
	}
#if 0
	if (iommu->ecap & ECAP_IR) {
		/* Interrupt remapping support */
		iommu_write_8(iommu, DMAR_IRTA_REG, 0);
	}
#endif

	/* Calculate guest address width and supported guest widths */
	gaw = -1;
	iommu->mgaw = cap_mgaw(iommu->cap);
	DPRINTF(0, "gaw: %d { ", iommu->mgaw);
	for (i = 0; i < 5; i++) {
		if (cap_sagaw(iommu->cap) & (1L << i)) {
			gaw = VTD_LEVELTOAW(i);
			DPRINTF(0, "%d ", gaw);
			iommu->agaw = gaw;
		}
	}
	DPRINTF(0, "}\n");

	/* Cache current status register bits */
	sts = iommu_read_4(iommu, DMAR_GSTS_REG);
	if (sts & GSTS_TES)
		iommu->gcmd |= GCMD_TE;
	if (sts & GSTS_QIES)
		iommu->gcmd |= GCMD_QIE;
	if (sts & GSTS_IRES)
		iommu->gcmd |= GCMD_IRE;
	DPRINTF(0, "gcmd: %x preset\n", iommu->gcmd);
	acpidmar_intr(iommu);
	return (0);
}

/* Read/Write IOMMU register */
uint32_t
iommu_read_4(struct iommu_softc *iommu, int reg)
{
	uint32_t	v;

	v = bus_space_read_4(iommu->iot, iommu->ioh, reg);
	return (v);
}

void
iommu_write_4(struct iommu_softc *iommu, int reg, uint32_t v)
{
	bus_space_write_4(iommu->iot, iommu->ioh, reg, (uint32_t)v);
}

uint64_t
iommu_read_8(struct iommu_softc *iommu, int reg)
{
	uint64_t	v;

	v = bus_space_read_8(iommu->iot, iommu->ioh, reg);
	return (v);
}

void
iommu_write_8(struct iommu_softc *iommu, int reg, uint64_t v)
{
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

	DPRINTF(0, "iommu%d: create domain: %.4x\n", iommu->id, did);
	dom = malloc(sizeof(*dom), M_DEVBUF, M_ZERO | M_WAITOK);
	dom->did = did;
	dom->iommu = iommu;
	dom->pte = iommu_alloc_page(iommu, &dom->ptep, 0);
	if (dom->pte == NULL) {
		free(dom, M_DEVBUF, sizeof(*dom));
		return NULL;
	}
	TAILQ_INIT(&dom->devices);

	/* Setup DMA */
	dom->dmat._cookie = dom;
	dom->dmat._dmamap_create = dmar_dmamap_create;		/* nop */
	dom->dmat._dmamap_destroy = dmar_dmamap_destroy;	/* nop */
	dom->dmat._dmamap_load = dmar_dmamap_load;		/* lm */
	dom->dmat._dmamap_load_mbuf = dmar_dmamap_load_mbuf;	/* lm */
	dom->dmat._dmamap_load_uio = dmar_dmamap_load_uio;	/* lm */
	dom->dmat._dmamap_load_raw = dmar_dmamap_load_raw;	/* lm */
	dom->dmat._dmamap_unload = dmar_dmamap_unload;		/* um */
	dom->dmat._dmamap_sync = _bus_dmamap_sync;		/* nop */
	dom->dmat._dmamem_alloc = dmar_dmamem_alloc;		/* nop */
	dom->dmat._dmamem_alloc_range = dmar_dmamem_alloc_range;	/* nop */
	dom->dmat._dmamem_free = dmar_dmamem_free;		/* nop */
	dom->dmat._dmamem_map = dmar_dmamem_map;		/* nop */
	dom->dmat._dmamem_unmap = dmar_dmamem_unmap;		/* nop */
	dom->dmat._dmamem_mmap = dmar_dmamem_mmap;

	snprintf(dom->exname, sizeof(dom->exname), "did:%x.%.4x",
	    iommu->id, dom->did);

	/* Setup IOMMU address map */
	gaw = min(iommu->agaw, iommu->mgaw);
	dom->iovamap = extent_create(dom->exname, 0, (1LL << gaw)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_NOCOALESCE);

	/*
	 * Some hardware, e.g. qwx(4), can't do DMA to low addresses.
	 * In addition, PCI-PCI bridges may block forwarding VGA
	 * memory addresses from the secondary to the primary
	 * interface.  Take the easy way out and reserve the first 1MB
	 * of the address space.  This also means we'll catch bugs
	 * where drivers inadvertedly do DMA to/from address zero.
	 */
	extent_alloc_region(dom->iovamap, RESERVED_ADDRESS, RESERVED_SIZE,
	    EX_WAITOK);

	/* Reserve MSI address window from IOVA allocations */
	extent_alloc_region(dom->iovamap, MSI_BASE_ADDRESS, MSI_BASE_SIZE,
	    EX_WAITOK);
	mtx_init(&dom->ptlck, IPL_HIGH);
	mtx_init(&dom->exlck, IPL_HIGH);

	/* Identity-map the MSI window */
	if (domain_map_pthru(dom, MSI_BASE_ADDRESS,
	    MSI_BASE_ADDRESS + MSI_BASE_SIZE) != 0) {
		printf("iommu%d: domain %.4x: failed to map MSI window\n",
		    iommu->id, dom->did);
		extent_destroy(dom->iovamap);
		iommu_free_page(dom->pte);
		free(dom, M_DEVBUF, sizeof(*dom));
		return NULL;
	}

	TAILQ_INSERT_TAIL(&iommu->domains, dom, link);

	return dom;
}

void
domain_add_device(struct domain *dom, int sid)
{
	struct domain_dev *ddev;
	struct ivmd_entry *ivmd;

	DPRINTF(0, "add %s to iommu%d.%.4x\n", dmar_bdf(sid), dom->iommu->id, dom->did);
	ddev = malloc(sizeof(*ddev), M_DEVBUF, M_ZERO | M_WAITOK);
	ddev->sid = sid;
	TAILQ_INSERT_TAIL(&dom->devices, ddev, link);

	TAILQ_FOREACH(ivmd, &acpidmar_sc->sc_ivmds, link) {
		if (sid < ivmd->start_id || sid > ivmd->end_id)
			continue;
		if ((ivmd->flags & (IVMD_EXCLRANGE | IVMD_UNITY)) == 0)
			continue;
		extent_alloc_region(dom->iovamap, ivmd->addr, ivmd->size,
		    EX_WAITOK | EX_CONFLICTOK);
		/* XXX use correct R/W permission for IVMD_UNITY */
		domain_map_pthru(dom, ivmd->addr, ivmd->addr + ivmd->size);
	}

	/* Should set context entry here?? */
}

void
domain_remove_device(struct domain *dom, int sid)
{
	struct domain_dev *ddev, *tmp;

	TAILQ_FOREACH_SAFE(ddev, &dom->devices, link, tmp) {
		if (ddev->sid == sid) {
			TAILQ_REMOVE(&dom->devices, ddev, link);
			free(ddev, M_DEVBUF, sizeof(*ddev));
		}
	}
}

/* Lookup domain by segment & source id (bus.device.function) */
struct domain *
domain_lookup(struct acpidmar_softc *sc, int segment, int sid)
{
	struct iommu_softc	*iommu;
	struct domain_dev	*ddev;
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
		if (rc != 0 || (iommu->flags & IOMMU_FLAGS_CATCHALL)) {
			break;
		}
	}
	if (!iommu) {
		printf("%s: no iommu found\n", dmar_bdf(sid));
		return NULL;
	}

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

/* Map Guest Pages into IOMMU */
void
_iommu_map(void *dom, vaddr_t va, bus_addr_t gpa, bus_size_t len)
{
	struct domain *d = dom;
	struct iommu_softc *iommu;
	bus_dma_segment_t seg;
	bus_size_t i;
	bus_addr_t start;
	paddr_t hpa;

	if (d == NULL)
		return;
	iommu = d->iommu;
	if (iommu == NULL || (iommu->flags & IOMMU_FLAGS_BAD))
		return;

	/* Ensure all devices in this domain have context/DTE programmed */
	domain_map_check(d);

	start = gpa;
	DPRINTF(1, "Mapping dma: %lx = %lx/%lx\n", va, gpa, len);
	for (i = 0; i < len; i += PAGE_SIZE) {
		hpa = 0;
		if (!pmap_extract(curproc->p_vmspace->vm_map.pmap, va, &hpa))
			break;
		if (domain_map_page(d, gpa, hpa, PTE_P | PTE_R | PTE_W, 0) != 0)
			break;
		gpa += PAGE_SIZE;
		va  += PAGE_SIZE;
	}

	if (i != 0 && iommu_enabled(iommu)) {
		seg.ds_addr = start;
		seg.ds_len = i;
		iommu_flush_write_buffer(iommu);
		iommu_flush_tlb_segs(iommu, d->did, &seg, 1);
	}
}

/* Find IOMMU for a given PCI device */
void
*_iommu_domain(int segment, int bus, int dev, int func, int *id)
{
	struct domain *dom;

	dom = domain_lookup(acpidmar_sc, segment, mksid(bus, dev, func));
	if (dom) {
		*id = dom->did;
	}
	return dom;
}

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
	/* AMD attach device */
	if (iommu->dte) {
		struct ivhd_dte *dte = &iommu->dte[sid];
		if (!dte->dw0) {
			/* Setup Device Table Entry: bus.devfn */
			DPRINTF(1, "@@@ PCI Attach: %.4x[%s] %.4x\n", sid, dmar_bdf(sid), dom->did);
			dte_set_host_page_table_root_ptr(dte, dom->ptep);
			dte_set_domain(dte, dom->did);
			dte_set_mode(dte, 3);  /* Set 3 level PTE */
			dte_set_tv(dte);
			dte_set_valid(dte);
			ivhd_flush_devtab(iommu, sid);
#ifdef IOMMU_DEBUG
			//ivhd_showreg(iommu);
			ivhd_showdte(iommu);
#endif
		}
		return;
	}

	/* Create Bus mapping */
	if (!root_entry_is_valid(&iommu->root[bus])) {
		iommu->ctx[bus] = iommu_alloc_page(iommu, &paddr, 0);
		if (iommu->ctx[bus] == NULL) {
			iommu->flags |= IOMMU_FLAGS_BAD;
			printf("iommu%d: can't allocate context for bus %.2x\n",
			    iommu->id, bus);
			return;
		}
		iommu->root[bus].lo = paddr | ROOT_P;
		iommu_flush_cache(iommu, &iommu->root[bus],
		    sizeof(struct root_entry));
		DPRINTF(0, "iommu%d: Allocate context for bus: %.2x pa:%.16llx va:%p\n",
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
		if ((iommu->cap & CAP_CM) || acpidmar_force_cm) {
			iommu_flush_ctx(iommu, CTX_DEVICE, dom->did, sid, 0);
			iommu_flush_tlb(iommu, IOTLB_GLOBAL, 0);
		} else {
			iommu_flush_write_buffer(iommu);
		}
		DPRINTF(0, "iommu%d: %s set context ptep:%.16llx lvl:%d did:%.4x tt:%d\n",
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

	if (mapctx) {
		domain_map_device(dom, sid);
	}

	return dom;
}

void
acpidmar_pci_hook(pci_chipset_tag_t pc, struct pci_attach_args *pa)
{
	int		bus, dev, fun, sid, rc;
	struct domain	*dom;
	pcireg_t	reg;

	if (!acpidmar_sc) {
		/* No DMAR, ignore */
		return;
	}

	/* Add device to our list if valid */
	pci_decompose_tag(pc, pa->pa_tag, &bus, &dev, &fun);
	sid = mksid(bus, dev, fun);
	if (sid_flag[sid] & SID_INVALID)
		return;

	reg = pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG);

	/* Add device to domain */
	dom = acpidmar_pci_attach(acpidmar_sc, pa->pa_domain, sid, 0);
	if (dom == NULL)
		return;

	if (PCI_CLASS(reg) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_DISPLAY_VGA) {
		dom->flag = DOM_NOMAP;
	}
	if (PCI_CLASS(reg) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_BRIDGE_ISA) {
		/* For ISA Bridges, map 0-16Mb as 1:1 */
		printf("dmar: %.4x:%.2x:%.2x.%x mapping ISA\n",
		    pa->pa_domain, bus, dev, fun);
		domain_map_pthru(dom, 0x00, 16*1024*1024);

		/* Keep the identity mapped IOVA range out of the allocator */
		rc = extent_alloc_region(dom->iovamap, 0,
		    16*1024*1024, EX_WAITOK | EX_CONFLICTOK);
		if (rc) {
			printf("%s: reserve ISA IOVA region failed (%d)\n",
			    dom->exname, rc);
		}
	}

	/*
	 * Ensure this device has a programmed context/DTE at attach time
	 * so they don't need a later allocation
	 */
	domain_map_device(dom, sid);

	if (iommu_bad(dom->iommu))
		return;

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

		DPRINTF(1, "  %8s  %.4x:%.2x.%.2x.%x {",
		    ds->type == DMAR_BRIDGE ? "bridge" : "endpoint",
		    segment, ds->bus,
		    d->dp[0].device,
		    d->dp[0].function);

		for (i = 1; i < d->ndp; i++) {
			DPRINTF(1, " %2x.%x ",
			    d->dp[i].device,
			    d->dp[i].function);
		}
		DPRINTF(1, "}\n");
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
	if (iommu_init(sc, iommu, &de->drhd) != 0) {
		printf("iommu init failed\n");
		free(iommu, M_DEVBUF, sizeof(*iommu));
		return;
	}

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
	uint64_t		limit;

	printf("RMRR: segment:%.4x range:%.16llx-%.16llx\n",
	    de->rmrr.segment, de->rmrr.base, de->rmrr.limit);
	if (de->rmrr.limit < de->rmrr.base) {
		printf("  buggy BIOS\n");
		return;
	}

	rmrr = malloc(sizeof(*rmrr), M_DEVBUF, M_ZERO | M_WAITOK);
	limit = de->rmrr.limit + 1;
	if (limit == 0)
		limit = de->rmrr.limit;

	rmrr->start = trunc_page(de->rmrr.base);
	rmrr->end = round_page(limit);
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

	domain_map_page = domain_map_page_intel;
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
			u_long len;

			sid = mksid(dl->bus, dl->dp[0].device,
			    dl->dp[0].function);
			dom = acpidmar_pci_attach(sc, rmrr->segment, sid, 0);
			if (dom != NULL) {
				printf("%s map ident: %.16llx %.16llx\n",
				    dom_bdf(dom), rmrr->start, rmrr->end);
				domain_map_pthru(dom, rmrr->start, rmrr->end);

				/* Keep the identity mapped region out of the IOVA allocator */
				len = rmrr->end - rmrr->start;
				rc = extent_alloc_region(dom->iovamap,
				    rmrr->start, len,
				    EX_WAITOK | EX_CONFLICTOK);
				if (rc) {
					printf("%s: rmrr extent reserve failed (%d)\n",
					    dom_bdf(dom), rc);
				}
			}
		}
	}
}

/*=====================================================
 * AMD Vi
 *=====================================================*/
void	acpiivrs_ivhd(struct acpidmar_softc *, struct acpi_ivhd *);
int	ivhd_iommu_init(struct acpidmar_softc *, struct iommu_softc *,
		struct acpi_ivhd *);
int	_ivhd_issue_command(struct iommu_softc *, const struct ivhd_command *);
void	ivhd_show_event(struct iommu_softc *, struct ivhd_event *evt, int);
int	ivhd_issue_command(struct iommu_softc *, const struct ivhd_command *, int);
int	ivhd_invalidate_domain(struct iommu_softc *, int);
void	ivhd_intr_map(struct iommu_softc *, int);
void	ivhd_checkerr(struct iommu_softc *iommu);
int	acpiivhd_intr(void *);

int
acpiivhd_intr(void *ctx)
{
	struct iommu_softc *iommu = ctx;

	if (!iommu->dte)
		return (0);
	ivhd_poll_events(iommu);
	return (1);
}

/* Setup interrupt for AMD */
void
ivhd_intr_map(struct iommu_softc *iommu, int devid) {
	pci_intr_handle_t ih;

	if (iommu->intr)
		return;
	ih.tag = pci_make_tag(NULL, sid_bus(devid), sid_dev(devid), sid_fun(devid));
	ih.line = APIC_INT_VIA_MSG;
	ih.pin = 0;
	iommu->intr = pci_intr_establish(NULL, ih, IPL_NET | IPL_MPSAFE,
				acpiivhd_intr, iommu, "amd_iommu");
	printf("amd iommu intr: %p\n", iommu->intr);
}

void
_dumppte(struct pte_entry *pte, int lvl, vaddr_t va)
{
	char *pfx[] = { "    ", "   ", "  ", " ", "" };
	uint64_t i, sh;
	struct pte_entry *npte;

	for (i = 0; i < 512; i++) {
		sh = (i << (((lvl-1) * 9) + 12));
		if (pte[i].val & PTE_P) {
			if (lvl > 1) {
				npte = (void *)PMAP_DIRECT_MAP((pte[i].val & PTE_PADDR_MASK));
				printf("%slvl%d: %.16llx nxt:%llu\n", pfx[lvl], lvl,
				    pte[i].val, (pte[i].val >> 9) & 7);
				_dumppte(npte, lvl-1, va | sh);
			} else {
				printf("%slvl%d: %.16llx <- %.16llx \n", pfx[lvl], lvl,
				    pte[i].val, va | sh);
			}
		}
	}
}

void
ivhd_showpage(struct iommu_softc *iommu, int sid, paddr_t paddr)
{
	struct domain *dom;
	static int show = 0;

	if (show > 10)
		return;
	show++;
	dom = acpidmar_pci_attach(acpidmar_sc, 0, sid, 0);
	if (!dom)
		return;
	printf("DTE: %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
	    iommu->dte[sid].dw0,
	    iommu->dte[sid].dw1,
	    iommu->dte[sid].dw2,
	    iommu->dte[sid].dw3,
	    iommu->dte[sid].dw4,
	    iommu->dte[sid].dw5,
	    iommu->dte[sid].dw6,
	    iommu->dte[sid].dw7);
	_dumppte(dom->pte, 3, 0);
}

/* Display AMD IOMMU Error */
void
ivhd_show_event(struct iommu_softc *iommu, struct ivhd_event *evt, int head)
{
	int type, sid, did, flag;
	uint64_t address;

	/* Get Device, Domain, Address and Type of event */
	sid  = __EXTRACT(evt->dw0, EVT_SID);
	type = __EXTRACT(evt->dw1, EVT_TYPE);
	did  = __EXTRACT(evt->dw1, EVT_DID);
	flag = __EXTRACT(evt->dw1, EVT_FLAG);
	address = _get64(&evt->dw2);

	printf("=== IOMMU Error[%.4x]: ", head);
	switch (type) {
	case ILLEGAL_DEV_TABLE_ENTRY:
		printf("illegal dev table entry dev=%s addr=0x%.16llx %s, %s, %s, %s\n",
		    dmar_bdf(sid), address,
		    evt->dw1 & EVT_TR ? "translation" : "transaction",
		    evt->dw1 & EVT_RZ ? "reserved bit" : "invalid level",
		    evt->dw1 & EVT_RW ? "write" : "read",
		    evt->dw1 & EVT_I  ? "interrupt" : "memory");
		ivhd_showdte(iommu);
		break;
	case IO_PAGE_FAULT:
		printf("io page fault dev=%s did=0x%.4x addr=0x%.16llx\n%s, %s, %s, %s, %s, %s\n",
		    dmar_bdf(sid), did, address,
		    evt->dw1 & EVT_TR ? "translation" : "transaction",
		    evt->dw1 & EVT_RZ ? "reserved bit" : "invalid level",
		    evt->dw1 & EVT_PE ? "no perm" : "perm",
		    evt->dw1 & EVT_RW ? "write" : "read",
		    evt->dw1 & EVT_PR ? "present" : "not present",
		    evt->dw1 & EVT_I  ? "interrupt" : "memory");
		ivhd_showdte(iommu);
		ivhd_showpage(iommu, sid, address);
		break;
	case DEV_TAB_HARDWARE_ERROR:
		printf("device table hardware error dev=%s addr=0x%.16llx %s, %s, %s\n",
		    dmar_bdf(sid), address,
		    evt->dw1 & EVT_TR ? "translation" : "transaction",
		    evt->dw1 & EVT_RW ? "write" : "read",
		    evt->dw1 & EVT_I  ? "interrupt" : "memory");
		ivhd_showdte(iommu);
		break;
	case PAGE_TAB_HARDWARE_ERROR:
		printf("page table hardware error dev=%s addr=0x%.16llx %s, %s, %s\n",
		    dmar_bdf(sid), address,
		    evt->dw1 & EVT_TR ? "translation" : "transaction",
		    evt->dw1 & EVT_RW ? "write" : "read",
		    evt->dw1 & EVT_I  ? "interrupt" : "memory");
		ivhd_showdte(iommu);
		break;
	case ILLEGAL_COMMAND_ERROR:
		printf("illegal command addr=0x%.16llx\n", address);
		ivhd_showcmd(iommu);
		break;
	case COMMAND_HARDWARE_ERROR:
		printf("command hardware error addr=0x%.16llx flag=0x%.4x\n",
		    address, flag);
		ivhd_showcmd(iommu);
		break;
	case IOTLB_INV_TIMEOUT:
		printf("iotlb invalidation timeout dev=%s address=0x%.16llx\n",
		    dmar_bdf(sid), address);
		break;
	case INVALID_DEVICE_REQUEST:
		printf("invalid device request dev=%s addr=0x%.16llx flag=0x%.4x\n",
		    dmar_bdf(sid), address, flag);
		break;
	default:
		printf("unknown type=0x%.2x\n", type);
		break;
	}
	/* Clear old event */
	evt->dw0 = 0;
	evt->dw1 = 0;
	evt->dw2 = 0;
	evt->dw3 = 0;
}

/* AMD: Process IOMMU error from hardware */
int
ivhd_poll_events(struct iommu_softc *iommu)
{
	uint32_t head, tail;
	int sz;

	sz = sizeof(struct ivhd_event);
	head = iommu_read_4(iommu, EVT_HEAD_REG);
	tail = iommu_read_4(iommu, EVT_TAIL_REG);
	if (head == tail) {
		/* No pending events */
		return (0);
	}
	while (head != tail) {
		iommu_flush_cache(iommu, iommu->evt_tbl + head, sz);
		ivhd_show_event(iommu, iommu->evt_tbl + head, head);
		head = (head + sz) % EVT_TBL_SIZE;
	}
	iommu_write_4(iommu, EVT_HEAD_REG, head);
	return (0);
}

/* AMD: Issue command to IOMMU queue */
int
_ivhd_issue_command(struct iommu_softc *iommu, const struct ivhd_command *cmd)
{
	uint32_t head, tail, next;
	int sz, n;

	sz = sizeof(*cmd);

	mtx_enter(&iommu->reg_lock);

	for (n = 0; ; n++) {
		head = iommu_read_4(iommu, CMD_HEAD_REG);
		tail = iommu_read_4(iommu, CMD_TAIL_REG);
		next = (tail + sz) % CMD_TBL_SIZE;
		if (next != head)
			break;

		mtx_leave(&iommu->reg_lock);
		if (n >= 1000) {
			printf("iommu%d: IVHD cmd queue full\n", iommu->id);
			return (-EBUSY);
		}
		delay(10);
		mtx_enter(&iommu->reg_lock);
	}

	memcpy(iommu->cmd_tbl + tail, cmd, sz);
	iommu_flush_cache(iommu, iommu->cmd_tbl + tail, sz);
	iommu_write_4(iommu, CMD_TAIL_REG, next);

	mtx_leave(&iommu->reg_lock);

	return (tail / sz);
}

#define IVHD_MAXDELAY 8
int
ivhd_issue_command(struct iommu_softc *iommu, const struct ivhd_command *cmd,
    int wait)
{
	int rc;

	rc = _ivhd_issue_command(iommu, cmd);
	if (rc >= 0 && wait)
		ivhd_completion_wait(iommu);
	return rc;
}

/* AMD: Flush changes to Device Table Entry for a specific device */
int
ivhd_flush_devtab(struct iommu_softc *iommu, int devid)
{
	struct ivhd_command cmd = {
	    .dw0 = devid,
	    .dw1 = INVALIDATE_DEVTAB_ENTRY << CMD_SHIFT
	};

	return ivhd_issue_command(iommu, &cmd, 1);
}

/* AMD: Invalidate all IOMMU device and page tables */
int
ivhd_invalidate_iommu_all(struct iommu_softc *iommu)
{
	struct ivhd_command cmd = {
	    .dw1 = INVALIDATE_IOMMU_ALL << CMD_SHIFT
	};

	return ivhd_issue_command(iommu, &cmd, 0);
}

/* AMD: Invalidate interrupt remapping */
int
ivhd_invalidate_interrupt_table(struct iommu_softc *iommu, int devid)
{
	struct ivhd_command cmd = {
	    .dw0 = devid,
	    .dw1 = INVALIDATE_INTERRUPT_TABLE << CMD_SHIFT
	};

	return ivhd_issue_command(iommu, &cmd, 0);
}

/* AMD: Invalidate all page tables in a domain */
int
ivhd_invalidate_domain(struct iommu_softc *iommu, int did)
{
	struct ivhd_command cmd = { .dw1 = did | (INVALIDATE_IOMMU_PAGES << CMD_SHIFT) };

	cmd.dw2 = 0xFFFFF000 | 0x3;
	cmd.dw3 = 0x7FFFFFFF;
	return ivhd_issue_command(iommu, &cmd, 1);
}

/*
 * AMD: Completion wait for batching
 *
 * Enqueue a COMPLETION_WAIT command and poll the completion variable until it
 * is updated by the IOMMU.
 */
static void
ivhd_completion_wait(struct iommu_softc *iommu)
{
	struct ivhd_command wq = { 0 };
	volatile uint64_t *wv;
	paddr_t paddr;
	uint64_t token;
	int rc, i;

	if (iommu->wait_tbl == NULL) {
		printf("iommu%d: missing completion wait buffer\n", iommu->id);
		return;
	}

	mtx_enter(&iommu->wait_lock);

	/* Clear completion state before issuing the wait command */
	wv = (volatile uint64_t *)iommu->wait_tbl;
	wv[0] = 0;
	wv[1] = 0;
	iommu_flush_cache(iommu, (void *)wv, sizeof(uint64_t) * 2);

	token = 0xfeedc0de00000000ULL | (uint64_t)++iommu->wait_seq;

	paddr = iommu->wait_tblp;
	wq.dw0 = (paddr & ~0xF) | 0x1; /* s = 1 (store) */
	wq.dw1 = (COMPLETION_WAIT << CMD_SHIFT) | ((paddr >> 32) & 0xFFFFF);
	wq.dw2 = (uint32_t)token;
	wq.dw3 = (uint32_t)(token >> 32);

	rc = _ivhd_issue_command(iommu, &wq);
	if (rc < 0) {
		printf("iommu%d: completion wait enqueue failed: %d\n",
		    iommu->id, rc);
		mtx_leave(&iommu->wait_lock);
		return;
	}

	/* wv[0] will be updated to token when the command completes */
	for (i = 0; i < IVHD_MAXDELAY; i++) {
		iommu_flush_cache(iommu, (void *)wv, sizeof(uint64_t) * 2);
		if (wv[0] == token)
			break;
		DELAY(10 << i);
	}
	if (i == IVHD_MAXDELAY) {
		printf("iommu%d: completion wait timeout\n", iommu->id);
	}

	mtx_leave(&iommu->wait_lock);
}

/* AMD: Invalidate a single 4KB page in a domain */
static int
ivhd_invalidate_page(struct iommu_softc *iommu, int did, bus_addr_t iova)
{
	struct ivhd_command cmd = { 0 };

	iova &= ~(bus_addr_t)VTD_PAGE_MASK;
	cmd.dw1 = did | (INVALIDATE_IOMMU_PAGES << CMD_SHIFT);
	cmd.dw2 = (uint32_t)(iova & ~0xfffULL);
	cmd.dw3 = (uint32_t)(iova >> 32);

	return ivhd_issue_command(iommu, &cmd, 1);
}

/* AMD: Invalidate translations for a list of IOVA ranges */
static void
ivhd_invalidate_segs(struct iommu_softc *iommu, int did, bus_dma_segment_t *segs,
    int nsegs)
{
	struct ivhd_command cmd = { 0 };
	bus_addr_t start, end, addr;
	int i, rc;

	if (segs == NULL || nsegs <= 0)
		return;

	for (i = 0; i < nsegs; i++) {
		if (segs[i].ds_len == 0)
			continue;
		start = segs[i].ds_addr & ~(bus_addr_t)VTD_PAGE_MASK;
		end = roundup(segs[i].ds_addr + segs[i].ds_len, VTD_PAGE_SIZE);

		for (addr = start; addr < end; addr += VTD_PAGE_SIZE) {
			memset(&cmd, 0, sizeof(cmd));
			cmd.dw1 = did | (INVALIDATE_IOMMU_PAGES << CMD_SHIFT);
			cmd.dw2 = (uint32_t)(addr & ~0xfffULL);
			cmd.dw3 = (uint32_t)(addr >> 32);

			rc = _ivhd_issue_command(iommu, &cmd);
			if (rc < 0) {
				printf("iommu%d: page invalidate enqueue "
				    "failed: %d\n", iommu->id, rc);
				ivhd_completion_wait(iommu);
				return;
			}
		}
	}

	ivhd_completion_wait(iommu);
}

/* AMD: Display Registers */
void
ivhd_showreg(struct iommu_softc *iommu)
{
	printf("---- dt:%.16llx cmd:%.16llx evt:%.16llx ctl:%.16llx sts:%.16llx\n",
	    iommu_read_8(iommu, DEV_TAB_BASE_REG),
	    iommu_read_8(iommu, CMD_BASE_REG),
	    iommu_read_8(iommu, EVT_BASE_REG),
	    iommu_read_8(iommu, IOMMUCTL_REG),
	    iommu_read_8(iommu, IOMMUSTS_REG));
	printf("---- cmd queue:%.16llx %.16llx evt queue:%.16llx %.16llx\n",
	    iommu_read_8(iommu, CMD_HEAD_REG),
	    iommu_read_8(iommu, CMD_TAIL_REG),
	    iommu_read_8(iommu, EVT_HEAD_REG),
	    iommu_read_8(iommu, EVT_TAIL_REG));
}

/* AMD: Generate Errors to test event handler */
void
ivhd_checkerr(struct iommu_softc *iommu)
{
	struct ivhd_command cmd = { -1, -1, -1, -1 };

	/* Generate ILLEGAL DEV TAB entry? */
	iommu->dte[0x2303].dw0 = -1;		/* invalid */
	iommu->dte[0x2303].dw2 = 0x1234;	/* domain */
	iommu->dte[0x2303].dw7 = -1;		/* reserved */
	ivhd_flush_devtab(iommu, 0x2303);
	ivhd_poll_events(iommu);

	/* Generate ILLEGAL_COMMAND_ERROR : ok */
	ivhd_issue_command(iommu, &cmd, 0);
	ivhd_poll_events(iommu);

	/* Generate page hardware error */
}

/* AMD: Show Device Table Entry */
void
ivhd_showdte(struct iommu_softc *iommu)
{
	int i;

	for (i = 0; i < 65536; i++) {
		if (iommu->dte[i].dw0) {
			printf("%.2x:%.2x.%x: %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
			    i >> 8, (i >> 3) & 0x1F, i & 0x7,
			    iommu->dte[i].dw0, iommu->dte[i].dw1,
			    iommu->dte[i].dw2, iommu->dte[i].dw3,
			    iommu->dte[i].dw4, iommu->dte[i].dw5,
			    iommu->dte[i].dw6, iommu->dte[i].dw7);
		}
	}
}

/* AMD: Show command entries */
void
ivhd_showcmd(struct iommu_softc *iommu)
{
	struct ivhd_command *ihd;
	paddr_t phd;
	int i;

	ihd = iommu->cmd_tbl;
	phd = iommu_read_8(iommu, CMD_BASE_REG) & CMD_BASE_MASK;
	for (i = 0; i < 4096 / 128; i++) {
		printf("%.2x: %.16llx %.8x %.8x %.8x %.8x\n", i,
		    (uint64_t)phd + i * sizeof(*ihd),
		    ihd[i].dw0,ihd[i].dw1,ihd[i].dw2,ihd[i].dw3);
	}
}

#define _c(x) (int)((iommu->ecap >> x ##_SHIFT) & x ## _MASK)

/* AMD: Initialize IOMMU */
int
ivhd_iommu_init(struct acpidmar_softc *sc, struct iommu_softc *iommu,
	struct acpi_ivhd *ivhd)
{
	static int niommu;
	paddr_t paddr;
	uint64_t ov;

	if (sc == NULL || iommu == NULL || ivhd == NULL) {
		printf("Bad pointer to iommu_init!\n");
		return -1;
	}
	if (_bus_space_map(sc->sc_memt, ivhd->address, 0x80000, 0, &iommu->ioh) != 0) {
		printf("Bus Space Map fails\n");
		return -1;
	}
	TAILQ_INIT(&iommu->domains);
	TAILQ_INIT(&iommu->devices);

	mtx_init(&iommu->reg_lock, IPL_HIGH);
	mtx_init(&iommu->wait_lock, IPL_NONE);

	/* Setup address width and number of domains */
	iommu->id = ++niommu;
	iommu->iot = sc->sc_memt;
	iommu->mgaw = 48;
	iommu->agaw = 48;
	iommu->flags = IOMMU_FLAGS_CATCHALL;
	if (ivhd->flags & IVHD_COHERENT)
		iommu->flags |= IOMMU_FLAGS_COHERENT;
	iommu->segment = 0;
	iommu->ndoms = 256;

	printf(": AMD iommu%d at 0x%.8llx\n", iommu->id, ivhd->address);

	iommu->ecap = iommu_read_8(iommu, EXTFEAT_REG);
	DPRINTF(0,"iommu%d: ecap:%.16llx ", iommu->id, iommu->ecap);
	DPRINTF(0,"%s%s%s%s%s%s%s%s\n",
	    iommu->ecap & EFR_PREFSUP ? "pref " : "",
	    iommu->ecap & EFR_PPRSUP  ? "ppr " : "",
	    iommu->ecap & EFR_NXSUP   ? "nx " : "",
	    iommu->ecap & EFR_GTSUP   ? "gt " : "",
	    iommu->ecap & EFR_IASUP   ? "ia " : "",
	    iommu->ecap & EFR_GASUP   ? "ga " : "",
	    iommu->ecap & EFR_HESUP   ? "he " : "",
	    iommu->ecap & EFR_PCSUP   ? "pc " : "");
	DPRINTF(0,"hats:%x gats:%x glxsup:%x smif:%x smifrc:%x gam:%x\n",
	    _c(EFR_HATS), _c(EFR_GATS), _c(EFR_GLXSUP), _c(EFR_SMIFSUP),
	    _c(EFR_SMIFRC), _c(EFR_GAMSUP));

	/* Turn off iommu */
	ov = iommu_read_8(iommu, IOMMUCTL_REG);
	iommu_write_8(iommu, IOMMUCTL_REG, ov & ~(CTL_IOMMUEN | CTL_COHERENT |
		CTL_HTTUNEN | CTL_RESPASSPW | CTL_PASSPW | CTL_ISOC));

	/* Enable intr, mark IOMMU device as invalid for remap */
	sid_flag[ivhd->devid] |= SID_INVALID;
	ivhd_intr_map(iommu, ivhd->devid);

	/* Setup command buffer with 4k buffer (128 entries) */
	iommu->cmd_tbl = iommu_alloc_page(iommu, &paddr, 0);
	if (iommu->cmd_tbl == NULL) {
		printf("iommu%d: can't allocate command buffer\n", iommu->id);
		iommu->flags |= IOMMU_FLAGS_BAD;
		return -1;
	}
	iommu_write_8(iommu, CMD_BASE_REG, (paddr & CMD_BASE_MASK) | CMD_TBL_LEN_4K);
	iommu_write_4(iommu, CMD_HEAD_REG, 0x00);
	iommu_write_4(iommu, CMD_TAIL_REG, 0x00);
	iommu->cmd_tblp = paddr;

	/* Setup event log with 4k buffer (128 entries) */
	iommu->evt_tbl = iommu_alloc_page(iommu, &paddr, 0);
	if (iommu->evt_tbl == NULL) {
		printf("iommu%d: can't allocate event log\n", iommu->id);
		iommu->flags |= IOMMU_FLAGS_BAD;
		return -1;
	}
	iommu_write_8(iommu, EVT_BASE_REG, (paddr & EVT_BASE_MASK) | EVT_TBL_LEN_4K);
	iommu_write_4(iommu, EVT_HEAD_REG, 0x00);
	iommu_write_4(iommu, EVT_TAIL_REG, 0x00);
	iommu->evt_tblp = paddr;

	/* Completion wait buffer */
	iommu->wait_tbl = iommu_alloc_page(iommu, &paddr, 0);
	if (iommu->wait_tbl == NULL) {
		printf("iommu%d: can't allocate completion wait buffer\n",
		    iommu->id);
		iommu->flags |= IOMMU_FLAGS_BAD;
		return -1;
	}
	iommu->wait_tblp = paddr;
	iommu->wait_seq = 0;

	/*
	 * Setup device table
	 * 1 entry per source ID (bus:device:function - 64k entries)
	 */
	if (sc->sc_hwdte == NULL) {
		printf("iommu%d: missing device table, disabling\n", iommu->id);
		iommu->flags |= IOMMU_FLAGS_BAD;
		return -1;
	}
	iommu->dte = sc->sc_hwdte;
	iommu_write_8(iommu, DEV_TAB_BASE_REG, (sc->sc_hwdtep & DEV_TAB_MASK) | DEV_TAB_LEN);

	/* Enable IOMMU */
	ov |= (CTL_IOMMUEN | CTL_EVENTLOGEN | CTL_CMDBUFEN | CTL_EVENTINTEN);
	if (ivhd->flags & IVHD_COHERENT)
		ov |= CTL_COHERENT;
	if (ivhd->flags & IVHD_HTTUNEN)
		ov |= CTL_HTTUNEN;
	if (ivhd->flags & IVHD_RESPASSPW)
		ov |= CTL_RESPASSPW;
	if (ivhd->flags & IVHD_PASSPW)
		ov |= CTL_PASSPW;
	if (ivhd->flags & IVHD_ISOC)
		ov |= CTL_ISOC;
	ov &= ~(CTL_INVTIMEOUT_MASK << CTL_INVTIMEOUT_SHIFT);
	ov |=  (CTL_INVTIMEOUT_10MS << CTL_INVTIMEOUT_SHIFT);
	iommu_write_8(iommu, IOMMUCTL_REG, ov);

	ivhd_invalidate_iommu_all(iommu);

	TAILQ_INSERT_TAIL(&sc->sc_drhds, iommu, link);
	return 0;
}

void
acpiivrs_ivhd(struct acpidmar_softc *sc, struct acpi_ivhd *ivhd)
{
	struct iommu_softc *iommu;
	struct acpi_ivhd_ext *ext;
	union acpi_ivhd_entry *ie;
	int start, off, dte, all_dte = 0;

	if (ivhd->type == IVRS_IVHD_EXT) {
		ext = (struct acpi_ivhd_ext *)ivhd;
		DPRINTF(0,"ivhd: %.2x %.2x %.4x %.4x:%s %.4x %.16llx %.4x %.8x %.16llx\n",
		    ext->type, ext->flags, ext->length,
		    ext->segment, dmar_bdf(ext->devid), ext->cap,
		    ext->address, ext->info,
		    ext->attrib, ext->efr);
		if (ext->flags & IVHD_PPRSUP)
			DPRINTF(0," PPRSup");
		if (ext->flags & IVHD_PREFSUP)
			DPRINTF(0," PreFSup");
		if (ext->flags & IVHD_COHERENT)
			DPRINTF(0," Coherent");
		if (ext->flags & IVHD_IOTLB)
			DPRINTF(0," Iotlb");
		if (ext->flags & IVHD_ISOC)
			DPRINTF(0," ISoc");
		if (ext->flags & IVHD_RESPASSPW)
			DPRINTF(0," ResPassPW");
		if (ext->flags & IVHD_PASSPW)
			DPRINTF(0," PassPW");
		if (ext->flags & IVHD_HTTUNEN)
			DPRINTF(0, " HtTunEn");
		if (ext->flags)
			DPRINTF(0,"\n");
		off = sizeof(*ext);
		iommu = malloc(sizeof(*iommu), M_DEVBUF, M_ZERO|M_WAITOK);
		ivhd_iommu_init(sc, iommu, ivhd);
	} else {
		DPRINTF(0,"ivhd: %.2x %.2x %.4x %.4x:%s %.4x %.16llx %.4x %.8x\n",
		    ivhd->type, ivhd->flags, ivhd->length,
		    ivhd->segment, dmar_bdf(ivhd->devid), ivhd->cap,
		    ivhd->address, ivhd->info,
		    ivhd->feature);
		if (ivhd->flags & IVHD_PPRSUP)
			DPRINTF(0," PPRSup");
		if (ivhd->flags & IVHD_PREFSUP)
			DPRINTF(0," PreFSup");
		if (ivhd->flags & IVHD_COHERENT)
			DPRINTF(0," Coherent");
		if (ivhd->flags & IVHD_IOTLB)
			DPRINTF(0," Iotlb");
		if (ivhd->flags & IVHD_ISOC)
			DPRINTF(0," ISoc");
		if (ivhd->flags & IVHD_RESPASSPW)
			DPRINTF(0," ResPassPW");
		if (ivhd->flags & IVHD_PASSPW)
			DPRINTF(0," PassPW");
		if (ivhd->flags & IVHD_HTTUNEN)
			DPRINTF(0, " HtTunEn");
		if (ivhd->flags)
			DPRINTF(0,"\n");
		off = sizeof(*ivhd);
	}
	while (off < ivhd->length) {
		ie = (void *)ivhd + off;
		switch (ie->type) {
		case IVHD_RESVD:
			DPRINTF(0," RESVD\n");
			off += sizeof(ie->resvd);
			break;
		case IVHD_ALL:
			all_dte = ie->all.data;
			DPRINTF(0," ALL %.4x\n", all_dte);
			off += sizeof(ie->all);
			break;
		case IVHD_SEL:
			dte = ie->sel.data;
			DPRINTF(0," SELECT: %s %.4x\n", dmar_bdf(ie->sel.devid), dte);
			off += sizeof(ie->sel);
			break;
		case IVHD_SOR:
			dte = ie->sor.data;
			start = ie->sor.devid;
			DPRINTF(0," SOR: %s %.4x\n", dmar_bdf(start), dte);
			off += sizeof(ie->sor);
			break;
		case IVHD_EOR:
			DPRINTF(0," EOR: %s\n", dmar_bdf(ie->eor.devid));
			off += sizeof(ie->eor);
			break;
		case IVHD_ALIAS_SEL:
			dte = ie->alias.data;
			DPRINTF(0," ALIAS: src=%s: ", dmar_bdf(ie->alias.srcid));
			DPRINTF(0," %s %.4x\n", dmar_bdf(ie->alias.devid), dte);
			off += sizeof(ie->alias);
			break;
		case IVHD_ALIAS_SOR:
			dte = ie->alias.data;
			DPRINTF(0," ALIAS_SOR: %s %.4x ", dmar_bdf(ie->alias.devid), dte);
			DPRINTF(0," src=%s\n", dmar_bdf(ie->alias.srcid));
			off += sizeof(ie->alias);
			break;
		case IVHD_EXT_SEL:
			dte = ie->ext.data;
			DPRINTF(0," EXT SEL: %s %.4x %.8x\n", dmar_bdf(ie->ext.devid),
			    dte, ie->ext.extdata);
			off += sizeof(ie->ext);
			break;
		case IVHD_EXT_SOR:
			dte = ie->ext.data;
			DPRINTF(0," EXT SOR: %s %.4x %.8x\n", dmar_bdf(ie->ext.devid),
			    dte, ie->ext.extdata);
			off += sizeof(ie->ext);
			break;
		case IVHD_SPECIAL:
			DPRINTF(0," SPECIAL\n");
			off += sizeof(ie->special);
			break;
		default:
			DPRINTF(0," 2:unknown %x\n", ie->type);
			off = ivhd->length;
			break;
		}
	}
}

void
acpiivrs_ivmd(struct acpidmar_softc *sc, struct acpi_ivmd *ivmd)
{
	struct ivmd_entry *entry;

	entry = malloc(sizeof(*entry), M_DEVBUF, M_WAITOK);

	switch (ivmd->type) {
	case IVRS_IVMD_ALL:
		entry->start_id = 0;
		entry->end_id = 0xffff;
		break;
	case IVRS_IVMD_SPECIFIED:
		entry->start_id = ivmd->devid;
		entry->end_id = ivmd->devid;
		break;
	case IVRS_IVMD_RANGE:
		entry->start_id = ivmd->devid;
		entry->end_id = ivmd->auxdata;
		break;
	}

	entry->flags = ivmd->flags;
	entry->addr = ivmd->start_address;
	entry->size = ivmd->block_length;

	TAILQ_INSERT_TAIL(&sc->sc_ivmds, entry, link);
}

void
acpiivrs_init(struct acpidmar_softc *sc, struct acpi_ivrs *ivrs)
{
	union acpi_ivrs_entry *ie;
	int off;

	if (!sc->sc_hwdte) {
		sc->sc_hwdte = iommu_alloc_hwdte(sc, HWDTE_SIZE, &sc->sc_hwdtep);
		if (sc->sc_hwdte == NULL) {
			printf("%s: can't allocate HWDTE, disabling AMD IOMMU\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	domain_map_page = domain_map_page_amd;
	DPRINTF(0,"IVRS Version: %d\n", ivrs->hdr.revision);
	DPRINTF(0," VA Size: %d\n",
	    (ivrs->ivinfo >> IVRS_VASIZE_SHIFT) & IVRS_VASIZE_MASK);
	DPRINTF(0," PA Size: %d\n",
	    (ivrs->ivinfo >> IVRS_PASIZE_SHIFT) & IVRS_PASIZE_MASK);

	TAILQ_INIT(&sc->sc_drhds);
	TAILQ_INIT(&sc->sc_rmrrs);
	TAILQ_INIT(&sc->sc_atsrs);

	DPRINTF(0,"======== IVRS\n");
	off = sizeof(*ivrs);
	while (off < ivrs->hdr.length) {
		ie = (void *)ivrs + off;
		switch (ie->type) {
		case IVRS_IVHD:
		case IVRS_IVHD_EXT:
			acpiivrs_ivhd(sc, &ie->ivhd);
			break;
		case IVRS_IVMD_ALL:
		case IVRS_IVMD_SPECIFIED:
		case IVRS_IVMD_RANGE:
			acpiivrs_ivmd(sc, &ie->ivmd);
			break;
		default:
			DPRINTF(0,"1:unknown: %x\n", ie->type);
			break;
		}
		off += ie->length;
	}
	DPRINTF(0,"======== End IVRS\n");
}

static int
acpiivhd_activate(struct iommu_softc *iommu, int act)
{
	switch (act) {
	case DVACT_SUSPEND:
		iommu->flags |= IOMMU_FLAGS_SUSPEND;
		break;
	case DVACT_RESUME:
		iommu->flags &= ~IOMMU_FLAGS_SUSPEND;
		break;
	}
	return (0);
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
			if (iommu->dte) {
				acpiivhd_activate(iommu, act);
				continue;
			}
			iommu_flush_write_buffer(iommu);
			iommu_set_rtaddr(iommu, iommu->rtaddr);
			iommu_write_4(iommu, DMAR_FEDATA_REG, iommu->fedata);
			iommu_write_4(iommu, DMAR_FEADDR_REG, iommu->feaddr);
			iommu_write_4(iommu, DMAR_FEUADDR_REG,
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
			if (iommu->dte) {
				acpiivhd_activate(iommu, act);
				continue;
			}
			iommu->flags |= IOMMU_FLAGS_SUSPEND;
			iommu_enable_translation(iommu, 0);
			iommu_showcfg(iommu, -1);
		}
		break;
	}
	return (0);
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
	if (memcmp(hdr->signature, DMAR_SIG, sizeof(DMAR_SIG) - 1) == 0)
		return (1);
	if (memcmp(hdr->signature, IVRS_SIG, sizeof(IVRS_SIG) - 1) == 0)
		return (1);

	return (0);
}

void
acpidmar_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpidmar_softc *sc = (void *)self;
	struct acpi_attach_args	*aaa = aux;
	struct acpi_dmar *dmar = (struct acpi_dmar *)aaa->aaa_table;
	struct acpi_ivrs *ivrs = (struct acpi_ivrs *)aaa->aaa_table;
	struct acpi_table_header *hdr;

	TAILQ_INIT(&sc->sc_ivmds);

	hdr = (struct acpi_table_header *)aaa->aaa_table;
	sc->sc_memt = aaa->aaa_memt;
	sc->sc_dmat = aaa->aaa_dmat;
	if (memcmp(hdr->signature, DMAR_SIG, sizeof(DMAR_SIG) - 1) == 0) {
		acpidmar_sc = sc;
		acpidmar_init(sc, dmar);
	}
	if (memcmp(hdr->signature, IVRS_SIG, sizeof(IVRS_SIG) - 1) == 0) {
		acpidmar_sc = sc;
		acpiivrs_init(sc, ivrs);
	}
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

	iommu_write_4(iommu, DMAR_FECTL_REG, FECTL_IM);
	iommu_read_4(iommu, DMAR_FECTL_REG);

	mtx_leave(&iommu->reg_lock);
}

void
acpidmar_msi_hwunmask(struct pic *pic, int pin)
{
	struct iommu_pic	*ip = (void *)pic;
	struct iommu_softc	*iommu = ip->iommu;

	printf("msi_hwunmask\n");

	mtx_enter(&iommu->reg_lock);

	iommu_write_4(iommu, DMAR_FECTL_REG, 0);
	iommu_read_4(iommu, DMAR_FECTL_REG);

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
	iommu_write_4(iommu, DMAR_FEDATA_REG, vec);
	iommu_write_4(iommu, DMAR_FEADDR_REG, iommu->feaddr);
	iommu_write_4(iommu, DMAR_FEUADDR_REG, iommu->feaddr >> 32);

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

	return intr_establish(-1, pic, 0, IST_PULSE, level, NULL, func, arg, what);
}

/* Intel: Handle DMAR Interrupt */
int
acpidmar_intr(void *ctx)
{
	struct iommu_softc		*iommu = ctx;
	struct fault_entry		fe;
	static struct fault_entry	ofe;
	int				fro, nfr, fri, i;
	uint32_t			sts;

	/*splassert(IPL_HIGH);*/

	if (!(iommu->gcmd & GCMD_TE)) {
		return (1);
	}
	mtx_enter(&iommu->reg_lock);
	sts = iommu_read_4(iommu, DMAR_FECTL_REG);
	sts = iommu_read_4(iommu, DMAR_FSTS_REG);

	if (!(sts & FSTS_PPF)) {
		mtx_leave(&iommu->reg_lock);
		return (1);
	}

	nfr = cap_nfr(iommu->cap);
	fro = cap_fro(iommu->cap);
	fri = (sts >> FSTS_FRI_SHIFT) & FSTS_FRI_MASK;
	for (i = 0; i < nfr; i++) {
		fe.hi = iommu_read_8(iommu, fro + (fri*16) + 8);
		if (!(fe.hi & FRCD_HI_F))
			break;

		fe.lo = iommu_read_8(iommu, fro + (fri*16));
		if (ofe.hi != fe.hi || ofe.lo != fe.lo) {
			iommu_showfault(iommu, fri, &fe);
			ofe.hi = fe.hi;
			ofe.lo = fe.lo;
		}
		fri = (fri + 1) % nfr;
	}

	iommu_write_4(iommu, DMAR_FSTS_REG, FSTS_PFO | FSTS_PPF);

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
	"Root Entry Reserved",		/* ok (root.lo |= 0x4) */
	"Context Entry Reserved",
	"Paging Entry Reserved",
	"Context Entry TT",
	"Reserved",
};

void iommu_showpte(uint64_t, int, uint64_t);

/* Intel: Show IOMMU page table entry */
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

/* Intel: Show IOMMU configuration */
void
iommu_showcfg(struct iommu_softc *iommu, int sid)
{
	int i, j, sts, cmd;
	struct context_entry *ctx;
	pcitag_t tag;
	pcireg_t clc;

	cmd = iommu_read_4(iommu, DMAR_GCMD_REG);
	sts = iommu_read_4(iommu, DMAR_GSTS_REG);
	printf("iommu%d: flags:%d root pa:%.16llx %s %s %s %.8x %.8x\n",
	    iommu->id, iommu->flags, iommu_read_8(iommu, DMAR_RTADDR_REG),
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

/* Intel: Show IOMMU fault */
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

