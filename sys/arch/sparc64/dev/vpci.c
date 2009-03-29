/*	$OpenBSD: vpci.c,v 1.4 2009/03/29 22:52:11 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/hypervisor.h>
#include <machine/openfirm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <sparc64/dev/viommuvar.h>

extern struct sparc_pci_chipset _sparc_pci_chipset;

struct vpci_range {
	u_int32_t	cspace;
	u_int32_t	child_hi;
	u_int32_t	child_lo;
	u_int32_t	phys_hi;
	u_int32_t	phys_lo;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct vpci_pbm {
	struct vpci_softc *vp_sc;
	uint64_t vp_devhandle;

	struct vpci_range *vp_range;
	pci_chipset_tag_t vp_pc;
	int vp_nrange;

	bus_space_tag_t		vp_memt;
	bus_space_tag_t		vp_iot;
	bus_dma_tag_t		vp_dmat;
	struct iommu_state	vp_is;
};

struct vpci_softc {
	struct device sc_dv;
	bus_dma_tag_t sc_dmat;
	bus_space_tag_t sc_bust;
};

int vpci_match(struct device *, void *, void *);
void vpci_attach(struct device *, struct device *, void *);
void vpci_init_iommu(struct vpci_softc *, struct vpci_pbm *);
int vpci_print(void *, const char *);

pci_chipset_tag_t vpci_alloc_chipset(struct vpci_pbm *, int,
    pci_chipset_tag_t);
bus_space_tag_t vpci_alloc_mem_tag(struct vpci_pbm *);
bus_space_tag_t vpci_alloc_io_tag(struct vpci_pbm *);
bus_space_tag_t vpci_alloc_bus_tag(struct vpci_pbm *, const char *,
    int, int, int);
bus_dma_tag_t vpci_alloc_dma_tag(struct vpci_pbm *);

pcireg_t vpci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void vpci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);

int vpci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
int vpci_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t,
    bus_size_t, int, bus_space_handle_t *);
paddr_t vpci_bus_mmap(bus_space_tag_t, bus_space_tag_t, bus_addr_t, off_t,
    int, int);
void *vpci_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int, int,
    int (*)(void *), void *, const char *);
void vpci_intr_ack(struct intrhand *);

int vpci_dmamap_create(bus_dma_tag_t, bus_dma_tag_t, bus_size_t, int,
    bus_size_t, bus_size_t, int, bus_dmamap_t *);
void vpci_dmamap_destroy(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
int vpci_dmamap_load(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    void *, bus_size_t, struct proc *, int);
void vpci_dmamap_unload(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
int vpci_dmamem_alloc(bus_dma_tag_t, bus_dma_tag_t, bus_size_t,
    bus_size_t, bus_size_t, bus_dma_segment_t *, int, int *, int);
int vpci_dmamem_map(bus_dma_tag_t, bus_dma_tag_t, bus_dma_segment_t *,
    int, size_t, caddr_t *, int);
void vpci_dmamem_unmap(bus_dma_tag_t, bus_dma_tag_t, caddr_t, size_t);

int
vpci_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char compat[32];

	if (strcmp(ma->ma_name, "pci") != 0)
		return (0);

	if (OF_getprop(ma->ma_node, "compatible", compat, sizeof(compat)) == -1)
		return (0);

	if (strcmp(compat, "SUNW,sun4v-pci") == 0)
		return (1);

	return (0);
}

void
vpci_attach(struct device *parent, struct device *self, void *aux)
{
	struct vpci_softc *sc = (struct vpci_softc *)self;
	struct mainbus_attach_args *ma = aux;
	struct pcibus_attach_args pba;
	struct vpci_pbm *pbm;
	int *busranges = NULL, nranges;

	sc->sc_dmat = ma->ma_dmatag;
	sc->sc_bust = ma->ma_bustag;

	pbm = malloc(sizeof(*pbm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (pbm == NULL)
		panic("vpci: can't alloc vpci pbm");

	pbm->vp_sc = sc;
	pbm->vp_devhandle = (ma->ma_reg[0].ur_paddr >> 32) & 0x0fffffff;

	if (getprop(ma->ma_node, "ranges", sizeof(struct vpci_range),
	    &pbm->vp_nrange, (void **)&pbm->vp_range))
		panic("vpci: can't get ranges");

	if (getprop(ma->ma_node, "bus-range", sizeof(int), &nranges,
	    (void **)&busranges))
		panic("vpci: can't get bus-range");

	printf(": bus %d to %d, ", busranges[0], busranges[1]);

	pbm->vp_memt = vpci_alloc_mem_tag(pbm);
	pbm->vp_iot = vpci_alloc_io_tag(pbm);
	pbm->vp_dmat = vpci_alloc_dma_tag(pbm);

	pbm->vp_pc = vpci_alloc_chipset(pbm, ma->ma_node, &_sparc_pci_chipset);
	pbm->vp_pc->bustag = pbm->vp_memt;

	vpci_init_iommu(sc, pbm);

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = busranges[0];
	pba.pba_pc = pbm->vp_pc;
	pba.pba_dmat = pbm->vp_dmat;
	pba.pba_memt = pbm->vp_memt;
	pba.pba_iot = pbm->vp_iot;
	pba.pba_pc->conf_read = vpci_conf_read;
	pba.pba_pc->conf_write = vpci_conf_write;
	pba.pba_pc->intr_map = vpci_intr_map;

	free(busranges, M_DEVBUF);

	config_found(&sc->sc_dv, &pba, vpci_print);
}

void
vpci_init_iommu(struct vpci_softc *sc, struct vpci_pbm *pbm)
{
	struct iommu_state *is = &pbm->vp_is;
	int tsbsize = 8;
	u_int32_t iobase = 0x80000000;
	char *name;

	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", sc->sc_dv.dv_xname);

	viommu_init(name, is, tsbsize, iobase);
	is->is_devhandle = pbm->vp_devhandle;
}

int
vpci_print(void *aux, const char *p)
{
	if (p == NULL)
		return (UNCONF);
	return (QUIET);
}

pcireg_t
vpci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	struct vpci_pbm *pbm = pc->cookie;
	uint64_t error_flag, data;

	hv_pci_config_get(pbm->vp_devhandle, PCITAG_OFFSET(tag), reg, 4,
	    &error_flag, &data);

	return (error_flag ? (pcireg_t)~0 : data);
}

void
vpci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	struct vpci_pbm *pbm = pc->cookie;
	uint64_t error_flag;

	hv_pci_config_put(pbm->vp_devhandle, PCITAG_OFFSET(tag), reg, 4,
            data, &error_flag);
}

/*
 * Bus-specific interrupt mapping
 */
int
vpci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct vpci_pbm *pbm = pa->pa_pc->cookie;
	uint64_t devhandle = pbm->vp_devhandle;
	uint64_t devino = INTINO(*ihp);
	uint64_t sysino;
	int err;

	if (*ihp != (pci_intr_handle_t)-1) {
		err = hv_intr_devino_to_sysino(devhandle, devino, &sysino);
		if (err != H_EOK)
			return (-1);

		KASSERT(sysino == INTVEC(sysino));
		*ihp = sysino;
		return (0);
	}

	return (-1);
}

bus_space_tag_t
vpci_alloc_mem_tag(struct vpci_pbm *pp)
{
	return (vpci_alloc_bus_tag(pp, "mem",
	    0x02,       /* 32-bit mem space (where's the #define???) */
	    ASI_PRIMARY, ASI_PRIMARY_LITTLE));
}

bus_space_tag_t
vpci_alloc_io_tag(struct vpci_pbm *pp)
{
	return (vpci_alloc_bus_tag(pp, "io",
	    0x01,       /* IO space (where's the #define???) */
	    ASI_PRIMARY, ASI_PRIMARY_LITTLE));
}

bus_space_tag_t
vpci_alloc_bus_tag(struct vpci_pbm *pbm, const char *name, int ss,
    int asi, int sasi)
{
	struct vpci_softc *sc = pbm->vp_sc;
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("vpci: could not allocate bus tag");

	snprintf(bt->name, sizeof(bt->name), "%s-pbm_%s(%d/%2.2x)",
	    sc->sc_dv.dv_xname, name, ss, asi);

	bt->cookie = pbm;
	bt->parent = sc->sc_bust;
	bt->default_type = ss;
	bt->asi = asi;
	bt->sasi = sasi;
	bt->sparc_bus_map = vpci_bus_map;
	bt->sparc_bus_mmap = vpci_bus_mmap;
	bt->sparc_intr_establish = vpci_intr_establish;
	return (bt);
}

bus_dma_tag_t
vpci_alloc_dma_tag(struct vpci_pbm *pbm)
{
	struct vpci_softc *sc = pbm->vp_sc;
	bus_dma_tag_t dt, pdt = sc->sc_dmat;

	dt = malloc(sizeof(*dt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dt == NULL)
		panic("vpci: could not alloc dma tag");

	dt->_cookie = pbm;
	dt->_parent = pdt;
	dt->_dmamap_create = vpci_dmamap_create;
	dt->_dmamap_destroy = viommu_dvmamap_destroy;
	dt->_dmamap_load = viommu_dvmamap_load;
	dt->_dmamap_load_raw = viommu_dvmamap_load_raw;
	dt->_dmamap_unload = viommu_dvmamap_unload;
	dt->_dmamap_sync = viommu_dvmamap_sync;
	dt->_dmamem_alloc = viommu_dvmamem_alloc;
	dt->_dmamem_free = viommu_dvmamem_free;
	return (dt);
}

pci_chipset_tag_t
vpci_alloc_chipset(struct vpci_pbm *pbm, int node, pci_chipset_tag_t pc)
{
	pci_chipset_tag_t npc;

	npc = malloc(sizeof *npc, M_DEVBUF, M_NOWAIT);
	if (npc == NULL)
		panic("vpci: could not allocate pci_chipset_tag_t");
	memcpy(npc, pc, sizeof *pc);
	npc->cookie = pbm;
	npc->rootnode = node;
	return (npc);
}

#define BUS_DMA_FIND_PARENT(t, fn)                                      \
        if (t->_parent == NULL)                                         \
                panic("null bus_dma parent (" #fn ")");                 \
        for (t = t->_parent; t->fn == NULL; t = t->_parent)             \
                if (t->_parent == NULL)                                 \
                        panic("no bus_dma " #fn " located");

int
vpci_dmamap_create(bus_dma_tag_t t, bus_dma_tag_t t0, bus_size_t size,
    int nsegments, bus_size_t maxsegsz, bus_size_t boundary, int flags,
    bus_dmamap_t *dmamap)
{
	struct vpci_pbm *vp = t->_cookie;

	return (viommu_dvmamap_create(t, t0, &vp->vp_is, size, nsegments,
	    maxsegsz, boundary, flags, dmamap));
}

int
vpci_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t offset,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	struct vpci_pbm *pbm = t->cookie;
	int i, ss = t->default_type;

	if (t->parent == 0 || t->parent->sparc_bus_map == 0)
		panic("vpci_bus_map: invalid parent");

	if (flags & BUS_SPACE_MAP_PROMADDRESS) {
		return ((*t->parent->sparc_bus_map)
		    (t, t0, offset, size, flags, hp));
	}

	for (i = 0; i < pbm->vp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->vp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->vp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->vp_range[i].phys_hi) << 32;
		return ((*t->parent->sparc_bus_map)
		    (t, t0, paddr, size, flags, hp));
	}

	return (EINVAL);
}

paddr_t
vpci_bus_mmap(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t paddr,
    off_t off, int prot, int flags)
{
	bus_addr_t offset = paddr;
	struct vpci_pbm *pbm = t->cookie;
	int i, ss = t->default_type;

	if (t->parent == 0 || t->parent->sparc_bus_mmap == 0)
		panic("vpci_bus_mmap: invalid parent");

	for (i = 0; i < pbm->vp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->vp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->vp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->vp_range[i].phys_hi<<32);
		return ((*t->parent->sparc_bus_mmap)
		    (t, t0, paddr, off, prot, flags));
	}

	return (-1);
}

void *
vpci_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int ihandle,
    int level, int flags, int (*handler)(void *), void *arg, const char *what)
{
	uint64_t sysino = INTVEC(ihandle);
	struct intrhand *ih;
	int err;

	ih = bus_intr_allocate(t0, handler, arg, ihandle, level,
	    NULL, NULL, what);
	if (ih == NULL)
		return (NULL);

	intr_establish(ih->ih_pil, ih);
	ih->ih_ack = vpci_intr_ack;

	err = hv_intr_settarget(sysino, cpus->ci_upaid);
	if (err != H_EOK)
		return (NULL);

	/* Clear pending interrupts. */
	err = hv_intr_setstate(sysino, INTR_IDLE);
	if (err != H_EOK)
		return (NULL);

	err = hv_intr_setenabled(sysino, INTR_ENABLED);
	if (err != H_EOK)
		return (NULL);

	return (ih);
}

void
vpci_intr_ack(struct intrhand *ih)
{
	hv_intr_setstate(ih->ih_number, INTR_IDLE);
}

const struct cfattach vpci_ca = {
	sizeof(struct vpci_softc), vpci_match, vpci_attach
};

struct cfdriver vpci_cd = {
	NULL, "vpci", DV_DULL
};
