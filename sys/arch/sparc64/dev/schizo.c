/*	$OpenBSD: schizo.c,v 1.1 2002/06/08 21:56:02 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/reboot.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/psl.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/psychoreg.h>
#include <sparc64/dev/psychovar.h>
#include <sparc64/sparc64/cache.h>

#define	SCZ_PCIA_MEM_MATCH		0x00040
#define	SCZ_PCIA_MEM_MASK		0x00048
#define	SCZ_PCIA_IO_MATCH		0x00050
#define	SCZ_PCIA_IO_MASK		0x00058
#define	SCZ_PCIB_MEM_MATCH		0x00060
#define	SCZ_PCIB_MEM_MASK		0x00068
#define	SCZ_PCIB_IO_MATCH		0x00070
#define	SCZ_PCIB_IO_MASK		0x00078

struct schizo_range {
	u_int32_t	cspace;
	u_int32_t	child_hi;
	u_int32_t	child_lo;
	u_int32_t	phys_hi;
	u_int32_t	phys_lo;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct schizo_pbm {
	struct schizo_softc *sp_sc;

	struct schizo_range *sp_range;
	pci_chipset_tag_t sp_pc;
	int sp_nreg;
	int sp_nrange;
	int sp_nintmap;

	bus_space_tag_t		sp_memt;
	bus_space_tag_t		sp_iot;
	bus_space_tag_t		sp_cfgt;
	bus_space_handle_t	sp_cfgh;
	bus_dma_tag_t		sp_dmat;
	int			sp_bus;
	int			sp_flags;
	int			sp_bus_a;
	bus_addr_t		sp_confpaddr;
};

extern struct sparc_pci_chipset _sparc_pci_chipset;

int schizo_match(struct device *, void *, void *);
void schizo_attach(struct device *, struct device *, void *);
void schizo_init(struct schizo_softc *, int);
int schizo_print(void *, const char *);

u_int64_t schizo_read(bus_addr_t);

struct schizo_softc {
	struct device sc_dv;
	int sc_node;
	bus_dma_tag_t sc_dmat;
	bus_space_tag_t sc_bust;
	bus_space_tag_t sc_bustag;
	struct iommu_state *sc_is;
	bus_addr_t sc_ctrl;
};

struct cfattach schizo_ca = {
	sizeof(struct schizo_softc), schizo_match, schizo_attach
};

struct cfdriver schizo_cd = {
	NULL, "schizo", DV_DULL
};

pci_chipset_tag_t schizo_alloc_chipset(struct schizo_pbm *, int,
    pci_chipset_tag_t);
bus_space_tag_t schizo_alloc_bus_tag(struct schizo_pbm *, int);
bus_dma_tag_t schizo_alloc_dma_tag(struct schizo_pbm *);

pcireg_t schizo_pci_conf_read(pci_chipset_tag_t pc, pcitag_t, int);
void schizo_pci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);
paddr_t schizo_bus_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);
int _schizo_bus_map(bus_space_tag_t, bus_type_t, bus_addr_t,
    bus_size_t, int, vaddr_t, bus_space_handle_t *);
void *schizo_intr_establish(bus_space_tag_t, int, int, int,
    int (*)(void *), void *);

int schizo_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
    bus_size_t, struct proc *, int);
void schizo_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
int schizo_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
    bus_dma_segment_t *, int, bus_size_t, int);
void schizo_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    bus_size_t, int);
int schizo_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
void schizo_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int schizo_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
void schizo_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);

int
schizo_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct mainbus_attach_args *ma = aux;
	char *str;

	if (strcmp(ma->ma_name, "pci") != 0)
		return (0);

	str = getpropstring(ma->ma_node, "model");
	if (strcmp(str, "schizo") == 0)
		return (1);

	str = getpropstring(ma->ma_node, "compatible");
	if (strcmp(str, "pci108e,8001") == 0)
		return (1);

	return (0);
}

void
schizo_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct schizo_softc *sc = (struct schizo_softc *)self;
	struct mainbus_attach_args *ma = aux;
	int busa;

	sc->sc_node = ma->ma_node;
	sc->sc_dmat = ma->ma_dmatag;
	sc->sc_bust = ma->ma_bustag;
	sc->sc_ctrl = ma->ma_reg[1].ur_paddr - 0x10000;

	if ((ma->ma_reg[0].ur_paddr & 0x00700000) == 0x00600000)
		busa = 1;
	else
		busa = 0;

	schizo_init(sc, busa);
}

void
schizo_init(sc, busa)
	struct schizo_softc *sc;
	int busa;
{
	struct schizo_pbm *pbm;
	struct pcibus_attach_args pba;
	int *busranges = NULL, nranges;
	u_int64_t match;

	pbm = (struct schizo_pbm *)malloc(sizeof(*pbm), M_DEVBUF, M_NOWAIT);
	if (pbm == NULL)
		panic("schizo: can't alloc schizo pbm");
	bzero(pbm, sizeof(*pbm));

	pbm->sp_sc = sc;
	pbm->sp_bus_a = busa;
	if (getprop(sc->sc_node, "ranges", sizeof(struct schizo_range),
	    &pbm->sp_nrange, (void **)&pbm->sp_range))
		panic("schizo: can't get ranges");

	if (getprop(sc->sc_node, "bus-range", sizeof(int), &nranges,
	    (void **)&busranges))
		panic("schizo: can't get bus-range");

	printf(": bus %c %d to %d\n", busa ? 'A' : 'B',
	    busranges[0], busranges[1]);

	pci_conf_setfunc(schizo_pci_conf_read, schizo_pci_conf_write);

	match = schizo_read(sc->sc_ctrl + 
	   (busa ? SCZ_PCIA_IO_MATCH : SCZ_PCIB_IO_MATCH));
	pbm->sp_confpaddr = match & ~0x8000000000000000UL;

	printf("config space %llx\n", pbm->sp_confpaddr);

	pbm->sp_memt = schizo_alloc_bus_tag(pbm, PCI_MEMORY_BUS_SPACE);
	pbm->sp_iot = schizo_alloc_bus_tag(pbm, PCI_IO_BUS_SPACE);
	pbm->sp_cfgt = schizo_alloc_bus_tag(pbm, PCI_CONFIG_BUS_SPACE);
	pbm->sp_dmat = schizo_alloc_dma_tag(pbm);

	if (bus_space_map2(sc->sc_bust, PCI_CONFIG_BUS_SPACE,
	    pbm->sp_confpaddr, 0x1000000, 0, 0, &pbm->sp_cfgh))
		panic("schizo: could not map config space");

	pbm->sp_pc = schizo_alloc_chipset(pbm, sc->sc_node,
	    &_sparc_pci_chipset);

	pba.pba_busname = "pci";
	pba.pba_bus = busranges[0];
	pba.pba_pc = pbm->sp_pc;
#if 0
	pba.pba_flags = pbm->sp_flags;
#endif
	pba.pba_dmat = pbm->sp_dmat;
	pba.pba_memt = pbm->sp_memt;
	pba.pba_iot = pbm->sp_iot;

	free(busranges, M_DEVBUF);

	config_found(&sc->sc_dv, &pba, schizo_print);
}

int
schizo_print(aux, p)
	void *aux;
	const char *p;
{
	if (p == NULL)
		return (UNCONF);
	return (QUIET);
}

bus_space_tag_t
schizo_alloc_bus_tag(pbm, type)
	struct schizo_pbm *pbm;
	int type;
{
	struct schizo_softc *sc = pbm->sp_sc;
	bus_space_tag_t bt;

	bt = (bus_space_tag_t)
		malloc(sizeof(struct sparc_bus_space_tag), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("schizo: could not allocate bus tag");

	bzero(bt, sizeof *bt);
	bt->cookie = pbm;
	bt->parent = sc->sc_bust;
	bt->type = type;
	bt->sparc_bus_map = _schizo_bus_map;
#if XXX
	bt->sparc_bus_mmap = schizo_bus_mmap;
	bt->sparc_intr_establish = schizo_intr_establish;
#endif
	return (bt);
}

bus_dma_tag_t
schizo_alloc_dma_tag(pbm)
	struct schizo_pbm *pbm;
{
	struct schizo_softc *sc = pbm->sp_sc;
	bus_dma_tag_t dt, pdt = sc->sc_dmat;

	dt = (bus_dma_tag_t)malloc(sizeof(struct sparc_bus_dma_tag),
	    M_DEVBUF, M_NOWAIT);
	if (dt == NULL)
		panic("schizo: could not alloc dma tag");

	bzero(dt, sizeof(*dt));
	dt->_cookie = pbm;
	dt->_parent = pdt;
#define PCOPY(x)        dt->x = pdt->x
	PCOPY(_dmamap_create);
	PCOPY(_dmamap_destroy);
	dt->_dmamap_load = schizo_dmamap_load;
	PCOPY(_dmamap_load_mbuf);
	PCOPY(_dmamap_load_uio);
	dt->_dmamap_load_raw = schizo_dmamap_load_raw;
	dt->_dmamap_unload = schizo_dmamap_unload;
	dt->_dmamap_sync = schizo_dmamap_sync;
	dt->_dmamem_alloc = schizo_dmamem_alloc;
	dt->_dmamem_free = schizo_dmamem_free;
	dt->_dmamem_map = schizo_dmamem_map;
	dt->_dmamem_unmap = schizo_dmamem_unmap;
	PCOPY(_dmamem_mmap);
#undef  PCOPY
	return (dt);
}

pci_chipset_tag_t
schizo_alloc_chipset(pbm, node, pc)
	struct schizo_pbm *pbm;
	int node;
	pci_chipset_tag_t pc;
{
	pci_chipset_tag_t npc;

	npc = malloc(sizeof *npc, M_DEVBUF, M_NOWAIT);
	if (npc == NULL)
		panic("could not allocate pci_chipset_tag_t");
	memcpy(npc, pc, sizeof *pc);
	npc->cookie = pbm; 
	npc->rootnode = node;
	npc->curnode = node;
	return (npc);
}

int
schizo_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	return (iommu_dvmamap_load(t, sc->sc_is, map, buf, buflen, p, flags));
}

void
schizo_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
        struct schizo_softc *sc = pbm->sp_sc;

        iommu_dvmamap_unload(t, sc->sc_is, map);
}

int
schizo_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs, flags;
	bus_size_t size;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	return (iommu_dvmamap_load_raw(t, sc->sc_is, map, segs, nsegs,
	    flags, size));
}

void
schizo_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	if (ops & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)) {
		/* Flush the CPU then the IOMMU */
		bus_dmamap_sync(t->_parent, map, offset, len, ops);
		iommu_dvmamap_sync(t, sc->sc_is, map, offset, len, ops);
	}
	if (ops & (BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)) {
		/* Flush the IOMMU then the CPU */
		iommu_dvmamap_sync(t, sc->sc_is, map, offset, len, ops);
		bus_dmamap_sync(t->_parent, map, offset, len, ops);
	}
}

int
schizo_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size;
	bus_size_t alignment;
	bus_size_t boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	return (iommu_dvmamem_alloc(t, sc->sc_is, size, alignment, boundary,
	    segs, nsegs, rsegs, flags));
}

void
schizo_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	iommu_dvmamem_free(t, sc->sc_is, segs, nsegs);
}

int
schizo_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	return (iommu_dvmamem_map(t, sc->sc_is, segs, nsegs, size,
	    kvap, flags));
}

void
schizo_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;
	struct schizo_softc *sc = pbm->sp_sc;

	iommu_dvmamem_unmap(t, sc->sc_is, kva, size);
}

int schizo_get_childspace(int);

int schizo_get_childspace(type)
	int type;
{
	if (type == PCI_CONFIG_BUS_SPACE)
		return (0x0);
	if (type == PCI_IO_BUS_SPACE)
		return (0x1);
	if (type == PCI_MEMORY_BUS_SPACE)
		return (0x2);
#if 0
	if (type == PCI_MEMORY64_BUS_SPACE)
		return (0x3);
#endif
	panic("schizo: unknown type %d", type);
}

int
_schizo_bus_map(t, btype, offset, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t offset;
	bus_size_t size;
	int flags;
	vaddr_t vaddr;
	bus_space_handle_t *hp;
{
	struct schizo_pbm *pbm = t->cookie;
	struct schizo_softc *sc = pbm->sp_sc;
	int i, ss;

	ss = schizo_get_childspace(t->type);

	if (btype == 0)
		btype = t->type;

	for (i = 0; i < pbm->sp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->sp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->sp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->sp_range[i].phys_hi) << 32;
		return (bus_space_map2(sc->sc_bust, btype, paddr,
		    size, flags, vaddr, hp));
	}

	return (EINVAL);
}

pcireg_t
schizo_pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	struct schizo_pbm *pbm = pc->cookie;
	pcireg_t val = ~0;

	if (PCITAG_NODE(tag) == -1)
		return (val);

	val = bus_space_read_4(pbm->sp_cfgt, pbm->sp_cfgh,
	    PCITAG_OFFSET(tag) + reg);
	
	printf("read: tag %llx reg %x -> %x\n", tag, reg, val);
	return (val);
}

void
schizo_pci_conf_write(pc, tag, reg, data)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{
	struct schizo_pbm *pbm = pc->cookie;

	if (PCITAG_NODE(tag) == -1)
		return;

	bus_space_write_4(pbm->sp_cfgt, pbm->sp_cfgh,
	    PCITAG_OFFSET(tag) + reg, data);
}

u_int64_t
schizo_read(adr)
	bus_addr_t adr;
{
	u_int64_t r;

	__asm__ __volatile__("ldxa [%1] %2, %0"
	    : "=r" (r)
	    : "r" (adr), "i" (ASI_PHYS_NON_CACHED)
	    : "memory");
	return (r);
}
