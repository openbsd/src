/*	$OpenBSD: schizo.c,v 1.8 2003/01/13 16:04:38 jason Exp $	*/

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
#include <sparc64/dev/schizoreg.h>
#include <sparc64/dev/schizovar.h>
#include <sparc64/sparc64/cache.h>

extern struct sparc_pci_chipset _sparc_pci_chipset;

int schizo_match(struct device *, void *, void *);
void schizo_attach(struct device *, struct device *, void *);
void schizo_init(struct schizo_softc *, int);
void schizo_init_iommu(struct schizo_softc *, struct schizo_pbm *);
int schizo_print(void *, const char *);

pci_chipset_tag_t schizo_alloc_chipset(struct schizo_pbm *, int,
    pci_chipset_tag_t);
bus_space_tag_t schizo_alloc_bus_tag(struct schizo_pbm *, int);
bus_dma_tag_t schizo_alloc_dma_tag(struct schizo_pbm *);

pcireg_t schizo_pci_conf_read(pci_chipset_tag_t pc, pcitag_t, int);
void schizo_pci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);
paddr_t schizo_bus_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);
int _schizo_bus_map(bus_space_tag_t, bus_type_t, bus_addr_t,
    bus_size_t, int, vaddr_t, bus_space_handle_t *);
void *_schizo_intr_establish(bus_space_tag_t, int, int, int,
    int (*)(void *), void *);
paddr_t _schizo_bus_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);

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
int schizo_get_childspace(int);

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

	if (bus_space_map(sc->sc_bust, ma->ma_reg[1].ur_paddr - 0x10000,
	    sizeof(struct schizo_regs), 0, &sc->sc_ctrlh)) {
		printf(": failed to map registers\n");
		return;
	}
	sc->sc_regs = (struct schizo_regs *)bus_space_vaddr(sc->sc_bust,
	    sc->sc_ctrlh);

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

	schizo_init_iommu(sc, pbm);

	match = bus_space_read_8(sc->sc_bust, sc->sc_ctrlh,
	    (busa ? SCZ_PCIA_IO_MATCH : SCZ_PCIB_IO_MATCH));
	pbm->sp_confpaddr = match & ~0x8000000000000000UL;

	pbm->sp_memt = schizo_alloc_bus_tag(pbm, PCI_MEMORY_BUS_SPACE);
	pbm->sp_iot = schizo_alloc_bus_tag(pbm, PCI_IO_BUS_SPACE);
	pbm->sp_cfgt = schizo_alloc_bus_tag(pbm, PCI_CONFIG_BUS_SPACE);
	pbm->sp_dmat = schizo_alloc_dma_tag(pbm);

	if (bus_space_map2(sc->sc_bust, PCI_CONFIG_BUS_SPACE,
	    pbm->sp_confpaddr, 0x1000000, 0, 0, &pbm->sp_cfgh))
		panic("schizo: could not map config space");

	pbm->sp_pc = schizo_alloc_chipset(pbm, sc->sc_node,
	    &_sparc_pci_chipset);

	pbm->sp_pc->conf_read = schizo_pci_conf_read;
	pbm->sp_pc->conf_write = schizo_pci_conf_write;

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

void
schizo_init_iommu(sc, pbm)
	struct schizo_softc *sc;
	struct schizo_pbm *pbm;
{
	struct iommu_state *is = &pbm->sp_is;
	char *name;

	is->is_bustag = pbm->sp_sc->sc_bust;
	if (pbm->sp_bus_a) {
		is->is_iommu = &pbm->sp_sc->sc_regs->pbm_a.iommu;
		is->is_sb[0] = &pbm->sp_sc->sc_regs->pbm_a.strbuf;
	} else {
		is->is_iommu = &pbm->sp_sc->sc_regs->pbm_b.iommu;
		is->is_sb[0] = &pbm->sp_sc->sc_regs->pbm_b.strbuf;
	}

#if 1
	/* XXX disable the streaming buffers for now */
	is->is_sb[0]->strbuf_ctl &= ~STRBUF_EN;
	is->is_sb[0] = NULL;
#endif
	is->is_sb[1] = NULL;

	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", sc->sc_dv.dv_xname);

	iommu_init(name, is, 128 * 1024, 0xc0000000);
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
	bt->sparc_bus_mmap = _schizo_bus_mmap;
	bt->sparc_intr_establish = _schizo_intr_establish;
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

	return (iommu_dvmamap_load(t, &pbm->sp_is, map, buf, buflen, p, flags));
}

void
schizo_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;

        iommu_dvmamap_unload(t, &pbm->sp_is, map);
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

	return (iommu_dvmamap_load_raw(t, &pbm->sp_is, map, segs, nsegs,
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

	if (ops & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)) {
		/* Flush the CPU then the IOMMU */
		bus_dmamap_sync(t->_parent, map, offset, len, ops);
		iommu_dvmamap_sync(t, &pbm->sp_is, map, offset, len, ops);
	}
	if (ops & (BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)) {
		/* Flush the IOMMU then the CPU */
		iommu_dvmamap_sync(t, &pbm->sp_is, map, offset, len, ops);
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

	return (iommu_dvmamem_alloc(t, &pbm->sp_is, size, alignment, boundary,
	    segs, nsegs, rsegs, flags));
}

void
schizo_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;

	iommu_dvmamem_free(t, &pbm->sp_is, segs, nsegs);
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

	return (iommu_dvmamem_map(t, &pbm->sp_is, segs, nsegs, size,
	    kvap, flags));
}

void
schizo_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{
	struct schizo_pbm *pbm = (struct schizo_pbm *)t->_cookie;

	iommu_dvmamem_unmap(t, &pbm->sp_is, kva, size);
}

int
schizo_get_childspace(type)
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

paddr_t
_schizo_bus_mmap(t, paddr, off, prot, flags)
	bus_space_tag_t t;
	bus_addr_t paddr;
	off_t off;
	int prot;
	int flags;
{
	bus_addr_t offset = paddr;
	struct schizo_pbm *pbm = t->cookie;
	struct schizo_softc *sc = pbm->sp_sc;
	int i, ss;

	ss = schizo_get_childspace(t->type);

	for (i = 0; i < pbm->sp_nrange; i++) {
		bus_addr_t paddr;

		if (((pbm->sp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pbm->sp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pbm->sp_range[i].phys_hi<<32);
		return (bus_space_mmap(sc->sc_bustag, paddr, off,
		    prot, flags));
	}

	return (-1);
}

pcireg_t
schizo_pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	struct schizo_pbm *pbm = pc->cookie;

	if (PCITAG_NODE(tag) == -1)
		return (~0);

	return (bus_space_read_4(pbm->sp_cfgt, pbm->sp_cfgh,
	    PCITAG_OFFSET(tag) + reg));
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

void *
_schizo_intr_establish(t, ihandle, level, flags, handler, arg)
	bus_space_tag_t t;
	int ihandle;
	int level;
	int flags;
	int (*handler)(void *);
	void *arg;
{
	struct schizo_pbm *pbm = t->cookie;
	struct schizo_softc *sc = pbm->sp_sc;	
	struct intrhand *ih = NULL;
	volatile u_int64_t *intrmapptr = NULL, *intrclrptr = NULL;
	int ino;
	long vec = INTVEC(ihandle);

	ih = (struct intrhand *)malloc(sizeof(struct intrhand), M_DEVBUF,
	    M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	vec = INTVEC(ihandle);
	ino = INTINO(vec);

	if (level == IPL_NONE)
		level = INTLEV(vec);
	if (level == IPL_NONE) {
		printf(": no IPL, setting IPL 2.\n");
		level = 2;
	}

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) == 0) {
		struct schizo_pbm_regs *pbmreg;

		pbmreg = pbm->sp_bus_a ? &sc->sc_regs->pbm_a :
		    &sc->sc_regs->pbm_b;
		intrmapptr = &pbmreg->imap[ino];
		intrclrptr = &pbmreg->iclr[ino];
	}

	ih->ih_map = intrmapptr;
	ih->ih_clr = intrclrptr;
	ih->ih_fun = handler;
	ih->ih_pil = level;
	ih->ih_number = ino;

	intr_establish(ih->ih_pil, ih);

	if (intrmapptr != NULL) {
		u_int64_t intrmap;

		intrmap = *intrmapptr;
		intrmap |= INTMAP_V;
		*intrmapptr = intrmap;
		intrmap = *intrmapptr;
		ih->ih_number |= intrmap & INTMAP_INR;
	}

	return (ih);
}

const struct cfattach schizo_ca = {
	sizeof(struct schizo_softc), schizo_match, schizo_attach
};

struct cfdriver schizo_cd = {
	NULL, "schizo", DV_DULL
};
