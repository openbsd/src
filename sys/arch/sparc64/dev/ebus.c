/*	$OpenBSD: ebus.c,v 1.10 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: ebus.c,v 1.24 2001/07/25 03:49:54 eeh Exp $	*/

/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * UltraSPARC 5 and beyond ebus support.
 *
 * note that this driver is not complete:
 *	- ebus2 dma code is completely unwritten
 *	- interrupt establish is written and appears to work
 *	- bus map code is written and appears to work
 */

#ifdef DEBUG
#define	EDB_PROM	0x01
#define EDB_CHILD	0x02
#define	EDB_INTRMAP	0x04
#define EDB_BUSMAP	0x08
#define EDB_BUSDMA	0x10
#define EDB_INTR	0x20
int ebus_debug = 0x0;
#define DPRINTF(l, s)   do { if (ebus_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/time.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/psychoreg.h>
#include <sparc64/dev/psychovar.h>
#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>
#include <sparc64/sparc64/cache.h>

int	ebus_match(struct device *, void *, void *);
void	ebus_attach(struct device *, struct device *, void *);

struct cfattach ebus_ca = {
	sizeof(struct ebus_softc), ebus_match, ebus_attach
};

struct cfdriver ebus_cd = {
	NULL, "ebus", DV_DULL
};


int	ebus_setup_attach_args(struct ebus_softc *, int,
	    struct ebus_attach_args *);
void	ebus_destroy_attach_args(struct ebus_attach_args *);
int	ebus_print(void *, const char *);
void	ebus_find_ino(struct ebus_softc *, struct ebus_attach_args *);
int	ebus_find_node(struct pci_attach_args *);

/*
 * here are our bus space and bus dma routines.
 */
static paddr_t ebus_bus_mmap(bus_space_tag_t, bus_space_tag_t, bus_addr_t,
    off_t, int, int);
static int _ebus_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t,
    bus_size_t, int, bus_space_handle_t *);
static void *ebus_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int,
    int, int (*)(void *), void *);
static int ebus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    struct proc *, int);
static void ebus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
static void ebus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    bus_size_t, int);
int ebus_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
void ebus_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int ebus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
void ebus_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
bus_space_tag_t ebus_alloc_mem_tag(struct ebus_softc *, bus_space_tag_t);
bus_space_tag_t ebus_alloc_io_tag(struct ebus_softc *, bus_space_tag_t);
bus_space_tag_t _ebus_alloc_bus_tag(struct ebus_softc *sc, const char *,
    bus_space_tag_t, int);


int
ebus_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	char name[10];
	int node;

	/* Only attach if there's a PROM node. */
	node = PCITAG_NODE(pa->pa_tag);
	if (node == -1) return (0);

	/* Match a real ebus */
	OF_getprop(node, "name", &name, sizeof(name));
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_EBUS &&
		strcmp(name, "ebus") == 0)
		return (1);

	/* Or a real ebus III */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_EBUSIII &&
		strcmp(name, "ebus") == 0)
		return (1);

	/* Or a PCI-ISA bridge XXX I hope this is on-board. */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA) {
		return (1);
	}

	return (0);
}

/*
 * attach an ebus and all it's children.  this code is modeled
 * after the sbus code which does similar things.
 */
void
ebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct ebus_softc *sc = (struct ebus_softc *)self;
	struct pci_attach_args *pa = aux;
	struct ebus_attach_args eba;
	struct ebus_interrupt_map_mask *immp;
	int node, nmapmask, error;

	printf("\n");

	sc->sc_memtag = ebus_alloc_mem_tag(sc, pa->pa_memt);
	sc->sc_iotag = ebus_alloc_io_tag(sc, pa->pa_iot);
	sc->sc_dmatag = ebus_alloc_dma_tag(sc, pa->pa_dmat);

	node = PCITAG_NODE(pa->pa_tag);
	if (node == -1)
		panic("could not find ebus node");

	sc->sc_node = node;

	/*
	 * fill in our softc with information from the prom
	 */
	sc->sc_intmap = NULL;
	sc->sc_range = NULL;
	error = getprop(node, "interrupt-map",
			sizeof(struct ebus_interrupt_map),
			&sc->sc_nintmap, (void **)&sc->sc_intmap);
	switch (error) {
	case 0:
		immp = &sc->sc_intmapmask;
		error = getprop(node, "interrupt-map-mask",
			    sizeof(struct ebus_interrupt_map_mask), &nmapmask,
			    (void **)&immp);
		if (error)
			panic("could not get ebus interrupt-map-mask");
		if (nmapmask != 1)
			panic("ebus interrupt-map-mask is broken");
		break;
	case ENOENT:
		break;
	default:
		panic("ebus interrupt-map: error %d", error);
		break;
	}

	error = getprop(node, "ranges", sizeof(struct ebus_ranges),
	    &sc->sc_nrange, (void **)&sc->sc_range);
	if (error)
		panic("ebus ranges: error %d", error);

	/*
	 * now attach all our children
	 */
	DPRINTF(EDB_CHILD, ("ebus node %08x, searching children...\n", node));
	for (node = firstchild(node); node; node = nextsibling(node)) {
		char *name = getpropstring(node, "name");

		if (ebus_setup_attach_args(sc, node, &eba) != 0) {
			printf("ebus_attach: %s: incomplete\n", name);
			continue;
		} else {
			DPRINTF(EDB_CHILD, ("- found child `%s', attaching\n",
			    eba.ea_name));
			(void)config_found(self, &eba, ebus_print);
		}
		ebus_destroy_attach_args(&eba);
	}
}

int
ebus_setup_attach_args(struct ebus_softc *sc, int node,
    struct ebus_attach_args *ea)
{
	int	n, rv;

	bzero(ea, sizeof(struct ebus_attach_args));
	rv = getprop(node, "name", 1, &n, (void **)&ea->ea_name);
	if (rv != 0)
		return (rv);
	ea->ea_name[n] = '\0';

	ea->ea_node = node;
	ea->ea_memtag = sc->sc_memtag;
	ea->ea_iotag = sc->sc_iotag;
	ea->ea_dmatag = sc->sc_dmatag;

	rv = getprop(node, "reg", sizeof(struct ebus_regs), &ea->ea_nregs,
	    (void **)&ea->ea_regs);
	if (rv)
		return (rv);

	rv = getprop(node, "address", sizeof(u_int32_t), &ea->ea_nvaddrs,
	    (void **)&ea->ea_vaddrs);
	if (rv != ENOENT) {
		if (rv)
			return (rv);

		if (ea->ea_nregs != ea->ea_nvaddrs)
			printf("ebus loses: device %s: %d regs and %d addrs\n",
			    ea->ea_name, ea->ea_nregs, ea->ea_nvaddrs);
	} else
		ea->ea_nvaddrs = 0;

	if (getprop(node, "interrupts", sizeof(u_int32_t), &ea->ea_nintrs,
	    (void **)&ea->ea_intrs))
		ea->ea_nintrs = 0;
	else
		ebus_find_ino(sc, ea);

	return (0);
}

void
ebus_destroy_attach_args(struct ebus_attach_args *ea)
{
	if (ea->ea_name)
		free((void *)ea->ea_name, M_DEVBUF);
	if (ea->ea_regs)
		free((void *)ea->ea_regs, M_DEVBUF);
	if (ea->ea_intrs)
		free((void *)ea->ea_intrs, M_DEVBUF);
	if (ea->ea_vaddrs)
		free((void *)ea->ea_vaddrs, M_DEVBUF);
}

int
ebus_print(void *aux, const char *p)
{
	struct ebus_attach_args *ea = aux;
	int i;

	if (p)
		printf("%s at %s", ea->ea_name, p);
	for (i = 0; i < ea->ea_nregs; i++)
		printf("%s %x-%x", i == 0 ? " addr" : ",",
		    ea->ea_regs[i].lo,
		    ea->ea_regs[i].lo + ea->ea_regs[i].size - 1);
	for (i = 0; i < ea->ea_nintrs; i++)
		printf(" ipl %d", ea->ea_intrs[i]);
	return (UNCONF);
}


/*
 * find the INO values for each interrupt and fill them in.
 *
 * for each "reg" property of this device, mask it's hi and lo
 * values with the "interrupt-map-mask"'s hi/lo values, and also
 * mask the interrupt number with the interrupt mask.  search the
 * "interrupt-map" list for matching values of hi, lo and interrupt
 * to give the INO for this interrupt.
 */
void
ebus_find_ino(struct ebus_softc *sc, struct ebus_attach_args *ea)
{
	u_int32_t hi, lo, intr;
	int i, j, k;

	if (sc->sc_nintmap == 0) {
		for (i = 0; i < ea->ea_nintrs; i++) {
			OF_mapintr(ea->ea_node, &ea->ea_intrs[i],
				sizeof(ea->ea_intrs[0]),
				sizeof(ea->ea_intrs[0]));
		}
		return;
	}

	DPRINTF(EDB_INTRMAP,
	    ("ebus_find_ino: searching %d interrupts", ea->ea_nintrs));

	for (j = 0; j < ea->ea_nintrs; j++) {

		intr = ea->ea_intrs[j] & sc->sc_intmapmask.intr;

		DPRINTF(EDB_INTRMAP,
		    ("; intr %x masked to %x", ea->ea_intrs[j], intr));
		for (i = 0; i < ea->ea_nregs; i++) {
			hi = ea->ea_regs[i].hi & sc->sc_intmapmask.hi;
			lo = ea->ea_regs[i].lo & sc->sc_intmapmask.lo;

			DPRINTF(EDB_INTRMAP,
			    ("; reg hi.lo %08x.%08x masked to %08x.%08x",
			    ea->ea_regs[i].hi, ea->ea_regs[i].lo, hi, lo));
			for (k = 0; k < sc->sc_nintmap; k++) {
				DPRINTF(EDB_INTRMAP,
				    ("; checking hi.lo %08x.%08x intr %x",
				    sc->sc_intmap[k].hi, sc->sc_intmap[k].lo,
				    sc->sc_intmap[k].intr));
				if (hi == sc->sc_intmap[k].hi &&
				    lo == sc->sc_intmap[k].lo &&
				    intr == sc->sc_intmap[k].intr) {
					ea->ea_intrs[j] =
					    sc->sc_intmap[k].cintr;
					DPRINTF(EDB_INTRMAP,
					    ("; FOUND IT! changing to %d\n",
					    sc->sc_intmap[k].cintr));
					goto next_intr;
				}
			}
		}
next_intr:;
	}
}

bus_space_tag_t
ebus_alloc_mem_tag(struct ebus_softc *sc, bus_space_tag_t parent)
{
        return (_ebus_alloc_bus_tag(sc, "mem", parent,
            0x02));	/* 32-bit mem space (where's the #define???) */
}

bus_space_tag_t
ebus_alloc_io_tag(struct ebus_softc *sc, bus_space_tag_t parent)
{
        return (_ebus_alloc_bus_tag(sc, "io", parent,
            0x01));	/* IO space (where's the #define???) */
}

/*
 * bus space and bus dma below here
 */
bus_space_tag_t
_ebus_alloc_bus_tag(struct ebus_softc *sc, const char *name,
    bus_space_tag_t parent, int ss)
{
	struct sparc_bus_space_tag *bt;

	bt = malloc(sizeof(*bt), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("could not allocate ebus bus tag");

	bzero(bt, sizeof *bt);
	snprintf(bt->name, sizeof(bt->name), "%s_%s",
		sc->sc_dev.dv_xname, name);
	bt->cookie = sc;
	bt->parent = parent;
	bt->default_type = ss;
	bt->asi = parent->asi;
	bt->sasi = parent->sasi;
	bt->sparc_bus_map = _ebus_bus_map;
	bt->sparc_bus_mmap = ebus_bus_mmap;
	bt->sparc_intr_establish = ebus_intr_establish;
	return (bt);
}

bus_dma_tag_t
ebus_alloc_dma_tag(struct ebus_softc *sc, bus_dma_tag_t pdt)
{
	bus_dma_tag_t dt;

	dt = (bus_dma_tag_t)
		malloc(sizeof(struct sparc_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	if (dt == NULL)
		panic("could not allocate ebus dma tag");

	bzero(dt, sizeof *dt);
	dt->_cookie = sc;
	dt->_parent = pdt;
#define PCOPY(x)	dt->x = pdt->x
	PCOPY(_dmamap_create);
	PCOPY(_dmamap_destroy);
	dt->_dmamap_load = ebus_dmamap_load;
	PCOPY(_dmamap_load_mbuf);
	PCOPY(_dmamap_load_uio);
	PCOPY(_dmamap_load_raw);
	dt->_dmamap_unload = ebus_dmamap_unload;
	dt->_dmamap_sync = ebus_dmamap_sync;
	dt->_dmamem_alloc = ebus_dmamem_alloc;
	dt->_dmamem_free = ebus_dmamem_free;
	dt->_dmamem_map = ebus_dmamem_map;
	dt->_dmamem_unmap = ebus_dmamem_unmap;
	PCOPY(_dmamem_mmap);
#undef	PCOPY
	sc->sc_dmatag = dt;
	return (dt);
}

/*
 * bus space support.  <sparc64/dev/psychoreg.h> has a discussion
 * about PCI physical addresses, which also applies to ebus.
 */
static int
_ebus_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t offset,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	struct ebus_softc *sc = t->cookie;
	bus_addr_t hi, lo;
	int i;

	DPRINTF(EDB_BUSMAP,
	    ("\n_ebus_bus_map: type %d off %016llx sz %x flags %d",
	    (int)t->default_type, (unsigned long long)offset, (int)size,
	    (int)flags));

	if (t->parent == 0 || t->parent->sparc_bus_map == 0) {
		printf("\n_ebus_bus_map: invalid parent");
		return (EINVAL);
	}

	t = t->parent;

	if (flags & BUS_SPACE_MAP_PROMADDRESS) {
		return ((*t->sparc_bus_map)
		    (t, t0, offset, size, flags, hp));
	}

	hi = offset >> 32UL;
	lo = offset & 0xffffffff;

	DPRINTF(EDB_BUSMAP, (" (hi %08x lo %08x)", (u_int)hi, (u_int)lo));
	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t pciaddr;

		if (hi != sc->sc_range[i].child_hi)
			continue;
		if (lo < sc->sc_range[i].child_lo ||
		    (lo + size) >
		      (sc->sc_range[i].child_lo + sc->sc_range[i].size))
			continue;

		if(((sc->sc_range[i].phys_hi >> 24) & 3) != t->default_type)
			continue;

		pciaddr = ((bus_addr_t)sc->sc_range[i].phys_mid << 32UL) |
				       sc->sc_range[i].phys_lo;
		pciaddr += lo;
		DPRINTF(EDB_BUSMAP,
		    ("\n_ebus_bus_map: mapping space %x paddr offset %qx "
		    "pciaddr %qx\n", (int)t->default_type,
		    (unsigned long long)offset, (unsigned long long)pciaddr));
		/* pass it onto the psycho */
                return ((*t->sparc_bus_map)(t, t0, pciaddr, size, flags, hp));
	}
	DPRINTF(EDB_BUSMAP, (": FAILED\n"));
	return (EINVAL);
}

static paddr_t
ebus_bus_mmap(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t paddr,
    off_t off, int prot, int flags)
{
	bus_addr_t offset = paddr;
	struct ebus_softc *sc = t->cookie;
	int i;

	if (t->parent == 0 || t->parent->sparc_bus_mmap == 0) {
		printf("\nebus_bus_mmap: invalid parent");
		return (-1);
        }

	t = t->parent;

	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t paddr =
		    ((bus_addr_t)sc->sc_range[i].child_hi << 32) |
		    sc->sc_range[i].child_lo;

		if (offset != paddr)
			continue;

		DPRINTF(EDB_BUSMAP, ("\n_ebus_bus_mmap: mapping paddr %qx\n",
		    (unsigned long long)paddr));
		return ((*t->sparc_bus_mmap)(t, t0, paddr, off, prot, flags));
	}

	return (-1);
}

/*
 * install an interrupt handler for a PCI device
 */
void *
ebus_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int pri, int level,
    int flags, int (*handler)(void *), void *arg)
{
	if (t->parent == 0 || t->parent->sparc_bus_mmap == 0) {
		printf("\nebus_bus_mmap: invalid parent");
		return (NULL);
        }

	t = t->parent;

	return ((*t->sparc_intr_establish)(t, t0, pri, level, flags,
	    handler, arg));
}

/*
 * bus dma support
 */
int
ebus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	return (bus_dmamap_load(t->_parent, map, buf, buflen, p, flags));
}

void
ebus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	bus_dmamap_unload(t->_parent, map);
}

void
ebus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t offset,
    bus_size_t len, int ops)
{
	bus_dmamap_sync(t->_parent, map, offset, len, ops);
}

int
ebus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	return (bus_dmamem_alloc(t->_parent, size, alignment, boundary, segs,
	    nsegs, rsegs, flags));
}

void
ebus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	bus_dmamem_free(t->_parent, segs, nsegs);
}

int
ebus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	return (bus_dmamem_map(t->_parent, segs, nsegs, size, kvap, flags));
}

void
ebus_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{
	return (bus_dmamem_unmap(t->_parent, kva, size));
}
