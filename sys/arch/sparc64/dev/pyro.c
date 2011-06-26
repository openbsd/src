/*	$OpenBSD: pyro.c,v 1.21 2011/06/26 20:32:36 kettenis Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
 * Copyright (c) 2003 Henric Jungheim
 * Copyright (c) 2007 Mark Kettenis
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/pyrovar.h>

#ifdef DEBUG
#define PDB_PROM        0x01
#define PDB_BUSMAP      0x02
#define PDB_INTR        0x04
#define PDB_CONF        0x08
int pyro_debug = ~0;
#define DPRINTF(l, s)   do { if (pyro_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

#define FIRE_RESET_GEN			0x7010

#define FIRE_RESET_GEN_XIR		0x0000000000000002L

#define FIRE_INTRMAP_INT_CNTRL_NUM_MASK	0x000003c0
#define FIRE_INTRMAP_INT_CNTRL_NUM0	0x00000040
#define FIRE_INTRMAP_INT_CNTRL_NUM1	0x00000080
#define FIRE_INTRMAP_INT_CNTRL_NUM2	0x00000100
#define FIRE_INTRMAP_INT_CNTRL_NUM3	0x00000200
#define FIRE_INTRMAP_T_JPID_SHIFT	26
#define FIRE_INTRMAP_T_JPID_MASK	0x7c000000

#define OBERON_INTRMAP_T_DESTID_SHIFT	21
#define OBERON_INTRMAP_T_DESTID_MASK	0x7fe00000

extern struct sparc_pci_chipset _sparc_pci_chipset;

int pyro_match(struct device *, void *, void *);
void pyro_attach(struct device *, struct device *, void *);
void pyro_init(struct pyro_softc *, int);
void pyro_init_iommu(struct pyro_softc *, struct pyro_pbm *);
int pyro_print(void *, const char *);

pci_chipset_tag_t pyro_alloc_chipset(struct pyro_pbm *, int,
    pci_chipset_tag_t);
bus_space_tag_t pyro_alloc_mem_tag(struct pyro_pbm *);
bus_space_tag_t pyro_alloc_io_tag(struct pyro_pbm *);
bus_space_tag_t pyro_alloc_config_tag(struct pyro_pbm *);
bus_space_tag_t _pyro_alloc_bus_tag(struct pyro_pbm *, const char *,
    int, int, int);
bus_dma_tag_t pyro_alloc_dma_tag(struct pyro_pbm *);

int pyro_conf_size(pci_chipset_tag_t, pcitag_t);
pcireg_t pyro_conf_read(pci_chipset_tag_t, pcitag_t, int);
void pyro_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);

int pyro_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
int _pyro_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t,
    bus_size_t, int, bus_space_handle_t *);
paddr_t _pyro_bus_mmap(bus_space_tag_t, bus_space_tag_t, bus_addr_t, off_t,
    int, int);
void *_pyro_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int, int,
    int (*)(void *), void *, const char *);

int pyro_dmamap_create(bus_dma_tag_t, bus_dma_tag_t, bus_size_t, int,
    bus_size_t, bus_size_t, int, bus_dmamap_t *);

#ifdef DDB
void pyro_xir(void *, int);
#endif

int
pyro_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char *str;

	if (strcmp(ma->ma_name, "pci") != 0)
		return (0);

	str = getpropstring(ma->ma_node, "compatible");
	if (strcmp(str, "pciex108e,80f0") == 0 ||
	    strcmp(str, "pciex108e,80f8") == 0)
		return (1);

	return (0);
}

void
pyro_attach(struct device *parent, struct device *self, void *aux)
{
	struct pyro_softc *sc = (struct pyro_softc *)self;
	struct mainbus_attach_args *ma = aux;
	char *str;
	int busa;

	sc->sc_node = ma->ma_node;
	sc->sc_dmat = ma->ma_dmatag;
	sc->sc_bust = ma->ma_bustag;
	sc->sc_csr = ma->ma_reg[0].ur_paddr;
	sc->sc_xbc = ma->ma_reg[1].ur_paddr;
	sc->sc_ign = INTIGN(ma->ma_upaid << INTMAP_IGN_SHIFT);

	if ((ma->ma_reg[0].ur_paddr & 0x00700000) == 0x00600000)
		busa = 1;
	else
		busa = 0;

	if (bus_space_map(sc->sc_bust, sc->sc_csr,
	    ma->ma_reg[0].ur_len, 0, &sc->sc_csrh)) {
		printf(": failed to map csr registers\n");
		return;
	}

	if (bus_space_map(sc->sc_bust, sc->sc_xbc,
	    ma->ma_reg[1].ur_len, 0, &sc->sc_xbch)) {
		printf(": failed to map xbc registers\n");
		return;
	}

	str = getpropstring(ma->ma_node, "compatible");
	if (strcmp(str, "pciex108e,80f8") == 0)
		sc->sc_oberon = 1;

	pyro_init(sc, busa);
}

void
pyro_init(struct pyro_softc *sc, int busa)
{
	struct pyro_pbm *pbm;
	struct pcibus_attach_args pba;
	int *busranges = NULL, nranges;

	pbm = malloc(sizeof(*pbm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (pbm == NULL)
		panic("pyro: can't alloc pyro pbm");

	pbm->pp_sc = sc;
	pbm->pp_bus_a = busa;

	if (getprop(sc->sc_node, "ranges", sizeof(struct pyro_range),
	    &pbm->pp_nrange, (void **)&pbm->pp_range))
		panic("pyro: can't get ranges");

	if (getprop(sc->sc_node, "bus-range", sizeof(int), &nranges,
	    (void **)&busranges))
		panic("pyro: can't get bus-range");

	printf(": \"%s\", rev %d, ign %x, bus %c %d to %d\n",
	    sc->sc_oberon ? "Oberon" : "Fire",
	    getpropint(sc->sc_node, "module-revision#", 0), sc->sc_ign,
	    busa ? 'A' : 'B', busranges[0], busranges[1]);

	printf("%s: ", sc->sc_dv.dv_xname);
	pyro_init_iommu(sc, pbm);

	pbm->pp_memt = pyro_alloc_mem_tag(pbm);
	pbm->pp_iot = pyro_alloc_io_tag(pbm);
	pbm->pp_cfgt = pyro_alloc_config_tag(pbm);
	pbm->pp_dmat = pyro_alloc_dma_tag(pbm);

	if (bus_space_map(pbm->pp_cfgt, 0, 0x10000000, 0, &pbm->pp_cfgh))
		panic("pyro: can't map config space");

	pbm->pp_pc = pyro_alloc_chipset(pbm, sc->sc_node, &_sparc_pci_chipset);

	pbm->pp_pc->bustag = pbm->pp_cfgt;
	pbm->pp_pc->bushandle = pbm->pp_cfgh;

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = busranges[0];
	pba.pba_pc = pbm->pp_pc;
#if 0
	pba.pba_flags = pbm->pp_flags;
#endif
	pba.pba_dmat = pbm->pp_dmat;
	pba.pba_memt = pbm->pp_memt;
	pba.pba_iot = pbm->pp_iot;
	pba.pba_pc->conf_size = pyro_conf_size;
	pba.pba_pc->conf_read = pyro_conf_read;
	pba.pba_pc->conf_write = pyro_conf_write;
	pba.pba_pc->intr_map = pyro_intr_map;

	free(busranges, M_DEVBUF);

#ifdef DDB
	db_register_xir(pyro_xir, sc);
#endif

	config_found(&sc->sc_dv, &pba, pyro_print);
}

void
pyro_init_iommu(struct pyro_softc *sc, struct pyro_pbm *pbm)
{
	struct iommu_state *is = &pbm->pp_is;
	int tsbsize = 7;
	u_int32_t iobase = -1;
	char *name;

	is->is_bustag = sc->sc_bust;

	if (bus_space_subregion(is->is_bustag, sc->sc_csrh,
	    0x40000, 0x100, &is->is_iommu)) {
		panic("pyro: unable to create iommu handle");
	}

	is->is_sb[0] = &pbm->pp_sb;
	is->is_sb[0]->sb_bustag = is->is_bustag;

	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", sc->sc_dv.dv_xname);

	/* On Oberon, we need to flush the cache. */
	if (sc->sc_oberon)
		is->is_flags |= IOMMU_FLUSH_CACHE;

	iommu_init(name, is, tsbsize, iobase);
}

int
pyro_print(void *aux, const char *p)
{
	if (p == NULL)
		return (UNCONF);
	return (QUIET);
}

int
pyro_conf_size(pci_chipset_tag_t pc, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
pyro_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	return (bus_space_read_4(pc->bustag, pc->bushandle,
	    (PCITAG_OFFSET(tag) << 4) + reg));
}

void
pyro_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
        bus_space_write_4(pc->bustag, pc->bushandle,
	    (PCITAG_OFFSET(tag) << 4) + reg, data);
}

/*
 * Bus-specific interrupt mapping
 */
int
pyro_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct pyro_pbm *pp = pa->pa_pc->cookie;
	struct pyro_softc *sc = pp->pp_sc;
	u_int dev;

	if (*ihp != (pci_intr_handle_t)-1) {
		*ihp |= sc->sc_ign;
		return (0);
	}

	/*
	 * We didn't find a PROM mapping for this interrupt.  Try to
	 * construct one ourselves based on the swizzled interrupt pin
	 * and the interrupt mapping for PCI slots documented in the
	 * UltraSPARC-IIi User's Manual.
	 */

	if (pa->pa_intrpin == 0)
		return (-1);

	/*
	 * This deserves some documentation.  Should anyone
	 * have anything official looking, please speak up.
	 */
	dev = pa->pa_device - 1;

	*ihp = (pa->pa_intrpin - 1) & INTMAP_PCIINT;
	*ihp |= (dev << 2) & INTMAP_PCISLOT;
	*ihp |= sc->sc_ign;

	return (0);
}

bus_space_tag_t
pyro_alloc_mem_tag(struct pyro_pbm *pp)
{
	return (_pyro_alloc_bus_tag(pp, "mem",
	    0x02,       /* 32-bit mem space (where's the #define???) */
	    ASI_PRIMARY, ASI_PRIMARY_LITTLE));
}

bus_space_tag_t
pyro_alloc_io_tag(struct pyro_pbm *pp)
{
	return (_pyro_alloc_bus_tag(pp, "io",
	    0x01,       /* IO space (where's the #define???) */
	    ASI_PHYS_NON_CACHED_LITTLE, ASI_PHYS_NON_CACHED));
}

bus_space_tag_t
pyro_alloc_config_tag(struct pyro_pbm *pp)
{
	return (_pyro_alloc_bus_tag(pp, "cfg",
	    0x00,       /* Config space (where's the #define???) */
	    ASI_PHYS_NON_CACHED_LITTLE, ASI_PHYS_NON_CACHED));
}

bus_space_tag_t
_pyro_alloc_bus_tag(struct pyro_pbm *pbm, const char *name, int ss,
    int asi, int sasi)
{
	struct pyro_softc *sc = pbm->pp_sc;
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("pyro: could not allocate bus tag");

	snprintf(bt->name, sizeof(bt->name), "%s-pbm_%s(%d/%2.2x)",
	    sc->sc_dv.dv_xname, name, ss, asi);

	bt->cookie = pbm;
	bt->parent = sc->sc_bust;
	bt->default_type = ss;
	bt->asi = asi;
	bt->sasi = sasi;
	bt->sparc_bus_map = _pyro_bus_map;
	bt->sparc_bus_mmap = _pyro_bus_mmap;
	bt->sparc_intr_establish = _pyro_intr_establish;
	return (bt);
}

bus_dma_tag_t
pyro_alloc_dma_tag(struct pyro_pbm *pbm)
{
	struct pyro_softc *sc = pbm->pp_sc;
	bus_dma_tag_t dt, pdt = sc->sc_dmat;

	dt = malloc(sizeof(*dt), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dt == NULL)
		panic("pyro: could not alloc dma tag");

	dt->_cookie = pbm;
	dt->_parent = pdt;
	dt->_dmamap_create	= pyro_dmamap_create;
	dt->_dmamap_destroy	= iommu_dvmamap_destroy;
	dt->_dmamap_load	= iommu_dvmamap_load;
	dt->_dmamap_load_raw	= iommu_dvmamap_load_raw;
	dt->_dmamap_unload	= iommu_dvmamap_unload;
	dt->_dmamap_sync	= iommu_dvmamap_sync;
	dt->_dmamem_alloc	= iommu_dvmamem_alloc;
	dt->_dmamem_free	= iommu_dvmamem_free;
	return (dt);
}

pci_chipset_tag_t
pyro_alloc_chipset(struct pyro_pbm *pbm, int node, pci_chipset_tag_t pc)
{
	pci_chipset_tag_t npc;

	npc = malloc(sizeof *npc, M_DEVBUF, M_NOWAIT);
	if (npc == NULL)
		panic("pyro: could not allocate pci_chipset_tag_t");
	memcpy(npc, pc, sizeof *pc);
	npc->cookie = pbm;
	npc->rootnode = node;
	return (npc);
}

int
pyro_dmamap_create(bus_dma_tag_t t, bus_dma_tag_t t0, bus_size_t size,
    int nsegments, bus_size_t maxsegsz, bus_size_t boundary, int flags,
    bus_dmamap_t *dmamp)
{
	struct pyro_pbm *pp = t->_cookie;

	return (iommu_dvmamap_create(t, t0, &pp->pp_sb, size, nsegments,
	    maxsegsz, boundary, flags, dmamp));
}

int
_pyro_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t offset,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	struct pyro_pbm *pbm = t->cookie;
	int i, ss;

	DPRINTF(PDB_BUSMAP, ("_pyro_bus_map: type %d off %qx sz %qx flags %d",
	    t->default_type,
	    (unsigned long long)offset,
	    (unsigned long long)size,
	    flags));

	ss = t->default_type;
	DPRINTF(PDB_BUSMAP, (" cspace %d", ss));

	if (t->parent == 0 || t->parent->sparc_bus_map == 0) {
		printf("\n_pyro_bus_map: invalid parent");
		return (EINVAL);
	}

	if (flags & BUS_SPACE_MAP_PROMADDRESS) {
		return ((*t->parent->sparc_bus_map)
		    (t, t0, offset, size, flags, hp));
	}

	for (i = 0; i < pbm->pp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->pp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->pp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->pp_range[i].phys_hi) << 32;
		return ((*t->parent->sparc_bus_map)
		    (t, t0, paddr, size, flags, hp));
	}

	return (EINVAL);
}

paddr_t
_pyro_bus_mmap(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t paddr,
    off_t off, int prot, int flags)
{
	bus_addr_t offset = paddr;
	struct pyro_pbm *pbm = t->cookie;
	int i, ss;

	ss = t->default_type;

	DPRINTF(PDB_BUSMAP, ("_pyro_bus_mmap: prot %d flags %d pa %qx\n",
	    prot, flags, (unsigned long long)paddr));

	if (t->parent == 0 || t->parent->sparc_bus_mmap == 0) {
		printf("\n_pyro_bus_mmap: invalid parent");
		return (-1);
	}

	for (i = 0; i < pbm->pp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->pp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->pp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->pp_range[i].phys_hi) << 32;
		return ((*t->parent->sparc_bus_mmap)
		    (t, t0, paddr, off, prot, flags));
	}

	return (-1);
}

void *
_pyro_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int ihandle,
    int level, int flags, int (*handler)(void *), void *arg, const char *what)
{
	struct pyro_pbm *pbm = t->cookie;
	struct pyro_softc *sc = pbm->pp_sc;
	struct intrhand *ih = NULL;
	volatile u_int64_t *intrmapptr = NULL, *intrclrptr = NULL;
	int ino;

	ino = INTINO(ihandle);

	if (level == IPL_NONE)
		level = INTLEV(ihandle);
	if (level == IPL_NONE) {
		printf(": no IPL, setting IPL 2.\n");
		level = 2;
	}

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) == 0) {
		u_int64_t *imap, *iclr;

		imap = bus_space_vaddr(sc->sc_bust, sc->sc_csrh) + 0x1000;
		iclr = bus_space_vaddr(sc->sc_bust, sc->sc_csrh) + 0x1400;
		intrmapptr = &imap[ino];
		intrclrptr = &iclr[ino];
		ino |= INTVEC(ihandle);
	}

	ih = bus_intr_allocate(t0, handler, arg, ino, level, intrmapptr,
	    intrclrptr, what);
	if (ih == NULL)
		return (NULL);

	intr_establish(ih->ih_pil, ih);

	if (intrmapptr != NULL) {
		u_int64_t intrmap;

		intrmap = *intrmapptr;
		intrmap &= ~FIRE_INTRMAP_INT_CNTRL_NUM_MASK;
		intrmap |= FIRE_INTRMAP_INT_CNTRL_NUM0;
		if (sc->sc_oberon) {
			intrmap &= ~OBERON_INTRMAP_T_DESTID_MASK;
			intrmap |= CPU_JUPITERID <<
			    OBERON_INTRMAP_T_DESTID_SHIFT;
		} else {
			intrmap &= ~FIRE_INTRMAP_T_JPID_MASK;
			intrmap |= CPU_UPAID << FIRE_INTRMAP_T_JPID_SHIFT;
		}
		intrmap |= INTMAP_V;
		*intrmapptr = intrmap;
		intrmap = *intrmapptr;
		ih->ih_number |= intrmap & INTMAP_INR;
	}

	return (ih);
}

#ifdef DDB
void
pyro_xir(void *arg, int cpu)
{
	struct pyro_softc *sc = arg;

	bus_space_write_8(sc->sc_bust, sc->sc_xbch, FIRE_RESET_GEN,
	    FIRE_RESET_GEN_XIR);
}
#endif

const struct cfattach pyro_ca = {
	sizeof(struct pyro_softc), pyro_match, pyro_attach
};

struct cfdriver pyro_cd = {
	NULL, "pyro", DV_DULL
};
