/*	$OpenBSD: pyro.c,v 1.3 2007/03/31 22:14:52 kettenis Exp $	*/

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

extern struct sparc_pci_chipset _sparc_pci_chipset;

int pyro_match(struct device *, void *, void *);
void pyro_attach(struct device *, struct device *, void *);
void pyro_init(struct pyro_softc *, int);
int pyro_print(void *, const char *);

pci_chipset_tag_t pyro_alloc_chipset(struct pyro_pbm *, int,
    pci_chipset_tag_t);
bus_space_tag_t pyro_alloc_mem_tag(struct pyro_pbm *);
bus_space_tag_t pyro_alloc_io_tag(struct pyro_pbm *);
bus_space_tag_t pyro_alloc_config_tag(struct pyro_pbm *);
bus_space_tag_t _pyro_alloc_bus_tag(struct pyro_pbm *, const char *,
    int, int, int);

int pyro_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
int _pyro_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t,
    bus_size_t, int, bus_space_handle_t *);
paddr_t _pyro_bus_mmap(bus_space_tag_t, bus_space_tag_t, bus_addr_t, off_t,
    int, int);
void *_pyro_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int, int,
    int (*)(void *), void *, const char *);

int
pyro_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char *str;

	if (strcmp(ma->ma_name, "pci") != 0)
		return (0);

	str = getpropstring(ma->ma_node, "compatible");
	if (strcmp(str, "pciex108e,80f0") == 0)
		return (1);

	return (0);
}

void
pyro_attach(struct device *parent, struct device *self, void *aux)
{
	struct pyro_softc *sc = (struct pyro_softc *)self;
	struct mainbus_attach_args *ma = aux;
	int busa;

	sc->sc_node = ma->ma_node;
	sc->sc_dmat = ma->ma_dmatag;
	sc->sc_bust = ma->ma_bustag;
	sc->sc_ign = INTIGN(ma->ma_upaid << INTMAP_IGN_SHIFT);

	if ((ma->ma_reg[0].ur_paddr & 0x00700000) == 0x00600000)
		busa = 1;
	else
		busa = 0;

	pyro_init(sc, busa);
}

void
pyro_init(struct pyro_softc *sc, int busa)
{
	struct pyro_pbm *pbm;
	struct pcibus_attach_args pba;
	int *busranges = NULL, nranges;

	pbm = (struct pyro_pbm *)malloc(sizeof(*pbm), M_DEVBUF, M_NOWAIT);
	if (pbm == NULL)
		panic("pyro: can't alloc pyro pbm");
	bzero(pbm, sizeof(*pbm));

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

	pbm->pp_memt = pyro_alloc_mem_tag(pbm);
	pbm->pp_iot = pyro_alloc_io_tag(pbm);
	pbm->pp_cfgt = pyro_alloc_config_tag(pbm);
#if 0
	pbm->pp_dmat = pyro_alloc_dma_tag(pbm);
#endif

	if (bus_space_map(pbm->pp_cfgt, 0, 0x10000000, 0, &pbm->pp_cfgh))
		panic("pyro: could not map config space");

	pbm->pp_pc = pyro_alloc_chipset(pbm, sc->sc_node, &_sparc_pci_chipset);

	pbm->pp_pc->bustag = pbm->pp_cfgt;
	pbm->pp_pc->bushandle = pbm->pp_cfgh;

	pba.pba_busname = "pci";
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = busranges[0];
	pba.pba_bridgetag = NULL;
	pba.pba_pc = pbm->pp_pc;
#if 0
	pba.pba_flags = pbm->pp_flags;
#endif
	pba.pba_dmat = pbm->pp_dmat;
	pba.pba_memt = pbm->pp_memt;
	pba.pba_iot = pbm->pp_iot;
	pba.pba_pc->intr_map = pyro_intr_map;

	free(busranges, M_DEVBUF);

	config_found(&sc->sc_dv, &pba, pyro_print);
}

int
pyro_print(void *aux, const char *p)
{
	if (p == NULL)
		return (UNCONF);
	return (QUIET);
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

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("pyro: could not allocate bus tag");

	bzero(bt, sizeof *bt);
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
		paddr |= ((bus_addr_t)pbm->pp_range[i].phys_hi<<32);
		return ((*t->parent->sparc_bus_mmap)
		    (t, t0, paddr, off, prot, flags));
	}

	return (-1);
}

void *
_pyro_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int ihandle,
    int level, int flags, int (*handler)(void *), void *arg, const char *what)
{
	return (NULL);
}

const struct cfattach pyro_ca = {
	sizeof(struct pyro_softc), pyro_match, pyro_attach
};

struct cfdriver pyro_cd = {
	NULL, "pyro", DV_DULL
};
