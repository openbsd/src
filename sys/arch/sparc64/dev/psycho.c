/*	$OpenBSD: psycho.c,v 1.22 2002/09/17 13:25:52 art Exp $	*/
/*	$NetBSD: psycho.c,v 1.39 2001/10/07 20:30:41 eeh Exp $	*/

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
 * Support for `psycho' and `psycho+' UPA to PCI bridge and 
 * UltraSPARC IIi and IIe `sabre' PCI controllers.
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

#ifdef DEBUG
#define PDB_PROM	0x01
#define PDB_BUSMAP	0x02
#define PDB_INTR	0x04
#define PDB_CONF	0x08
int psycho_debug = 0;
#define DPRINTF(l, s)   do { if (psycho_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

pci_chipset_tag_t psycho_alloc_chipset(struct psycho_pbm *, int,
    pci_chipset_tag_t);
void psycho_get_bus_range(int, int *);
void psycho_get_ranges(int, struct psycho_ranges **, int *);
void psycho_set_intr(struct psycho_softc *, int, void *, 
    u_int64_t *, u_int64_t *);

/* Interrupt handlers */
int psycho_ue(void *);
int psycho_ce(void *);
int psycho_bus_a(void *);
int psycho_bus_b(void *);
int psycho_bus_error(struct psycho_softc *, int);
int psycho_powerfail(void *);
int psycho_wakeup(void *);

/* IOMMU support */
void psycho_iommu_init(struct psycho_softc *, int);

/*
 * bus space and bus dma support for UltraSPARC `psycho'.  note that most
 * of the bus dma support is provided by the iommu dvma controller.
 */
pcireg_t psycho_pci_conf_read(pci_chipset_tag_t pc, pcitag_t, int);
void psycho_pci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);
paddr_t psycho_bus_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);
int _psycho_bus_map(bus_space_tag_t, bus_type_t, bus_addr_t,
    bus_size_t, int, vaddr_t, bus_space_handle_t *);
void *psycho_intr_establish(bus_space_tag_t, int, int, int,
    int (*)(void *), void *);

int psycho_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
    bus_size_t, struct proc *, int);
void psycho_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
int psycho_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
    bus_dma_segment_t *, int, bus_size_t, int);
void psycho_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    bus_size_t, int);
int psycho_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
    bus_dma_segment_t *, int, int *, int);
void psycho_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int psycho_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
    caddr_t *, int);
void psycho_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);

/* base pci_chipset */
extern struct sparc_pci_chipset _sparc_pci_chipset;

/*
 * autoconfiguration
 */
int	psycho_match(struct device *, void *, void *);
void	psycho_attach(struct device *, struct device *, void *);
int	psycho_print(void *aux, const char *p);
int	psycho_get_childspace(int);


struct cfattach psycho_ca = {
        sizeof(struct psycho_softc), psycho_match, psycho_attach
};

struct cfdriver psycho_cd = {
	NULL, "psycho", DV_DULL
};

/*
 * "sabre" is the UltraSPARC IIi onboard UPA to PCI bridge.  It manages a
 * single PCI bus and does not have a streaming buffer.  It often has an APB
 * (advanced PCI bridge) connected to it, which was designed specifically for
 * the IIi.  The APB let's the IIi handle two independednt PCI buses, and
 * appears as two "simba"'s underneath the sabre.
 *
 * "psycho" and "psycho+" is a dual UPA to PCI bridge.  It sits on the UPA bus
 * and manages two PCI buses.  "psycho" has two 64-bit 33MHz buses, while
 * "psycho+" controls both a 64-bit 33Mhz and a 64-bit 66Mhz PCI bus.  You
 * will usually find a "psycho+" since I don't think the original "psycho"
 * ever shipped, and if it did it would be in the U30.  
 *
 * Each "psycho" PCI bus appears as a separate OFW node, but since they are
 * both part of the same IC, they only have a single register space.  As such,
 * they need to be configured together, even though the autoconfiguration will
 * attach them separately.
 *
 * On UltraIIi machines, "sabre" itself usually takes pci0, with "simba" often
 * as pci1 and pci2, although they have been implemented with other PCI bus
 * numbers on some machines.
 *
 * On UltraII machines, there can be any number of "psycho+" ICs, each
 * providing two PCI buses.  
 *
 *
 * XXXX The psycho/sabre node has an `interrupts' attribute.  They contain
 * the values of the following interrupts in this order:
 *
 * PCI Bus Error	(30)
 * DMA UE		(2e)
 * DMA CE		(2f)
 * Power Fail		(25)
 *
 * We really should attach handlers for each.
 *
 */
#define	ROM_PCI_NAME		"pci"

struct psycho_type {
	char *p_name;
	int p_type;
} psycho_types[] = {
	{ "SUNW,psycho",        PSYCHO_MODE_PSYCHO      },
	{ "pci108e,8000",       PSYCHO_MODE_PSYCHO      },
	{ "SUNW,sabre",         PSYCHO_MODE_SABRE       },
	{ "pci108e,a000",       PSYCHO_MODE_SABRE       },
	{ "pci108e,a001",       PSYCHO_MODE_SABRE       },
	{ NULL, 0 }
};

int
psycho_match(parent, match, aux)
	struct device	*parent;
	void		*match;
	void		*aux;
{
	struct mainbus_attach_args *ma = aux;
	struct psycho_type *ptype;
	char *str;

	/* match on a name of "pci" and a sabre or a psycho */
	if (strcmp(ma->ma_name, ROM_PCI_NAME) != 0)
		return (0);

	for (ptype = psycho_types; ptype->p_name != NULL; ptype++) {
		str = getpropstring(ma->ma_node, "model");
		if (strcmp(str, ptype->p_name) == 0)
			return (1);
		str = getpropstring(ma->ma_node, "compatible");
		if (strcmp(str, ptype->p_name) == 0)
			return (1);
	}
	return (0);
}

/*
 * SUNW,psycho initialisation ..
 *	- find the per-psycho registers
 *	- figure out the IGN.
 *	- find our partner psycho
 *	- configure ourselves
 *	- bus range, bus, 
 *	- get interrupt-map and interrupt-map-mask
 *	- setup the chipsets.
 *	- if we're the first of the pair, initialise the IOMMU, otherwise
 *	  just copy it's tags and addresses.
 */
void
psycho_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct psycho_softc *sc = (struct psycho_softc *)self;
	struct psycho_softc *osc = NULL;
	struct psycho_pbm *pp;
	struct pcibus_attach_args pba;
	struct mainbus_attach_args *ma = aux;
	bus_space_handle_t bh;
	u_int64_t csr;
	int psycho_br[2], n;
	struct pci_ctl *pci_ctl;
	struct psycho_type *ptype;

	printf("\n");

	sc->sc_node = ma->ma_node;
	sc->sc_bustag = ma->ma_bustag;
	sc->sc_dmatag = ma->ma_dmatag;

	/*
	 * call the model-specific initialisation routine.
	 */

	for (ptype = psycho_types; ptype->p_name != NULL; ptype++) {
		char *str;

		str = getpropstring(ma->ma_node, "model");
		if (strcmp(str, ptype->p_name) == 0)
			break;
		str = getpropstring(ma->ma_node, "compatible");
		if (strcmp(str, ptype->p_name) == 0)
			break;
	}
	if (ptype->p_name == NULL)
		panic("psycho_attach: unknown model?");
	sc->sc_mode = ptype->p_type;

	/*
	 * The psycho gets three register banks:
	 * (0) per-PBM configuration and status registers
	 * (1) per-PBM PCI configuration space, containing only the
	 *     PBM 256-byte PCI header
	 * (2) the shared psycho configuration registers (struct psychoreg)
	 *
	 * XXX use the prom address for the psycho registers?  we do so far.
	 */

	/* Register layouts are different.  stuupid. */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
		sc->sc_basepaddr = (paddr_t)ma->ma_reg[2].ur_paddr;

		if (ma->ma_naddress > 2) {
			sc->sc_regs = (struct psychoreg *)
			    (u_long)ma->ma_address[2];
			pci_ctl = (struct pci_ctl *)
			    (u_long)ma->ma_address[0];
		} else if (ma->ma_nreg > 2) {
			bus_space_handle_t handle;

			/* We need to map this in ourselves. */
			if (bus_space_map2(sc->sc_bustag, 0,
			    ma->ma_reg[2].ur_paddr, ma->ma_reg[2].ur_len,
			    0, NULL, &handle))
				panic("psycho_attach: cannot map regs");
			sc->sc_regs = (struct psychoreg *)(u_long)handle;

			if (bus_space_map2(sc->sc_bustag, 0,
			    ma->ma_reg[0].ur_paddr, ma->ma_reg[0].ur_len,
			    0, NULL, &handle))
				panic("psycho_attach: cannot map ctl");
/* XXX -- this is lost but never unmapped */
			pci_ctl = (struct pci_ctl *)(u_long)handle;
		} else
			panic("psycho_attach: %d not enough registers",
			    ma->ma_nreg);
	} else {
		sc->sc_basepaddr = (paddr_t)ma->ma_reg[0].ur_paddr;

		if (ma->ma_naddress) {
			sc->sc_regs = (struct psychoreg *)
			    (u_long)ma->ma_address[0];
			pci_ctl = (struct pci_ctl *)&sc->sc_regs->psy_pcictl[0];
		} else if (ma->ma_nreg) {
			bus_space_handle_t handle;

			/* We need to map this in ourselves. */
			if (bus_space_map2(sc->sc_bustag, 0,
			    ma->ma_reg[0].ur_paddr, ma->ma_reg[0].ur_len,
			    0, NULL, &handle))
				panic("psycho_attach: cannot map regs");
			sc->sc_regs = (struct psychoreg *)(u_long)handle;
			pci_ctl = (struct pci_ctl *)&sc->sc_regs->psy_pcictl[0];
		} else
			panic("psycho_attach: %d not enough registers",
			    ma->ma_nreg);
	}

	csr = sc->sc_regs->psy_csr;
	sc->sc_ign = 0x7c0; /* APB IGN is always 0x7c */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO)
		sc->sc_ign = PSYCHO_GCSR_IGN(csr) << 6;

	printf("%s: impl %d, version %d: ign %x ", ptype->p_name,
	    PSYCHO_GCSR_IMPL(csr), PSYCHO_GCSR_VERS(csr), sc->sc_ign);

	/*
	 * Match other psycho's that are already configured against
	 * the base physical address. This will be the same for a
	 * pair of devices that share register space.
	 */
	for (n = 0; n < psycho_cd.cd_ndevs; n++) {
		struct psycho_softc *asc =
		    (struct psycho_softc *)psycho_cd.cd_devs[n];

		if (asc == NULL || asc == sc)
			/* This entry is not there or it is me */
			continue;

		if (asc->sc_basepaddr != sc->sc_basepaddr)
			/* This is an unrelated psycho */
			continue;

		/* Found partner */
		osc = asc;
		break;
	}


	/* Oh, dear.  OK, lets get started */

	/*
	 * Setup the PCI control register
	 */
	csr = bus_space_read_8(sc->sc_bustag,
	    (bus_space_handle_t)(u_long)&pci_ctl->pci_csr, 0);
	csr |= PCICTL_MRLM | PCICTL_ARB_PARK | PCICTL_ERRINTEN |
	    PCICTL_4ENABLE;
	csr &= ~(PCICTL_SERR | PCICTL_CPU_PRIO | PCICTL_ARB_PRIO |
	    PCICTL_RTRYWAIT);
	bus_space_write_8(sc->sc_bustag,
	    (bus_space_handle_t)(u_long)&pci_ctl->pci_csr, 0, csr);


	/*
	 * Allocate our psycho_pbm
	 */
	pp = sc->sc_psycho_this = malloc(sizeof *pp, M_DEVBUF, M_NOWAIT);
	if (pp == NULL)
		panic("could not allocate psycho pbm");

	memset(pp, 0, sizeof *pp);

	pp->pp_sc = sc;

	/* grab the psycho ranges */
	psycho_get_ranges(sc->sc_node, &pp->pp_range, &pp->pp_nrange);

	/* get the bus-range for the psycho */
	psycho_get_bus_range(sc->sc_node, psycho_br);

	pba.pba_bus = psycho_br[0];

	printf("bus range %u to %u", psycho_br[0], psycho_br[1]);
	printf("; PCI bus %d", psycho_br[0]);

	pci_conf_setfunc(psycho_pci_conf_read, psycho_pci_conf_write);

	pp->pp_pcictl = &sc->sc_regs->psy_pcictl[0];

	/* allocate our tags */
	pp->pp_memt = psycho_alloc_mem_tag(pp);
	pp->pp_iot = psycho_alloc_io_tag(pp);
	pp->pp_dmat = psycho_alloc_dma_tag(pp);
	pp->pp_flags = (pp->pp_memt ? PCI_FLAGS_MEM_ENABLED : 0) |
	    (pp->pp_iot ? PCI_FLAGS_IO_ENABLED : 0);

	/* allocate a chipset for this */
	pp->pp_pc = psycho_alloc_chipset(pp, sc->sc_node, &_sparc_pci_chipset);

	/* setup the rest of the psycho pbm */
	pba.pba_pc = pp->pp_pc;

	printf("\n");

	/*
	 * And finally, if we're a sabre or the first of a pair of psycho's to
	 * arrive here, start up the IOMMU and get a config space tag.
	 */
	if (osc == NULL) {
		/*
		 * Establish handlers for interesting interrupts....
		 *
		 * XXX We need to remember these and remove this to support
		 * hotplug on the UPA/FHC bus.
		 *
		 * XXX Not all controllers have these, but installing them
		 * is better than trying to sort through this mess.
		 */
		psycho_set_intr(sc, 15, psycho_ue,
		    &sc->sc_regs->ue_int_map,
		    &sc->sc_regs->ue_clr_int);
		psycho_set_intr(sc, 1, psycho_ce,
		    &sc->sc_regs->ce_int_map,
		    &sc->sc_regs->ce_clr_int);
		psycho_set_intr(sc, 15, psycho_bus_a,
		    &sc->sc_regs->pciaerr_int_map,
		    &sc->sc_regs->pciaerr_clr_int);
		psycho_set_intr(sc, 15, psycho_bus_b,
		    &sc->sc_regs->pciberr_int_map,
		    &sc->sc_regs->pciberr_clr_int);
#if 0
		psycho_set_intr(sc, 15, psycho_powerfail,
		    &sc->sc_regs->power_int_map, 
		    &sc->sc_regs->power_clr_int);
#endif
		psycho_set_intr(sc, 1, psycho_wakeup,
		    &sc->sc_regs->pwrmgt_int_map, 
		    &sc->sc_regs->pwrmgt_clr_int);

		/*
		 * Setup IOMMU and PCI configuration if we're the first
		 * of a pair of psycho's to arrive here.
		 *
		 * We should calculate a TSB size based on amount of RAM
		 * and number of bus controllers and number an type of
		 * child devices.
		 *
		 * For the moment, 32KB should be more than enough.
		 */
		sc->sc_is = malloc(sizeof(struct iommu_state),
			M_DEVBUF, M_NOWAIT);
		if (sc->sc_is == NULL)
			panic("psycho_attach: malloc iommu_state");


		sc->sc_is->is_sb[0] = NULL;
		sc->sc_is->is_sb[1] = NULL;
		if (getproplen(sc->sc_node, "no-streaming-cache") >= 0)
			sc->sc_is->is_sb[0] = &pci_ctl->pci_strbuf;

		psycho_iommu_init(sc, 2);

		sc->sc_configtag = psycho_alloc_config_tag(sc->sc_psycho_this);
		if (bus_space_map2(sc->sc_bustag,
		    PCI_CONFIG_BUS_SPACE, sc->sc_basepaddr + 0x01000000,
		    0x0100000, 0, 0, &bh))
			panic("could not map psycho PCI configuration space");
		sc->sc_configaddr = (off_t)bh;
	} else {
		/* Just copy IOMMU state, config tag and address */
		sc->sc_is = osc->sc_is;
		sc->sc_configtag = osc->sc_configtag;
		sc->sc_configaddr = osc->sc_configaddr;

		if (getproplen(sc->sc_node, "no-streaming-cache") >= 0)
			sc->sc_is->is_sb[1] = &pci_ctl->pci_strbuf;
		iommu_reset(sc->sc_is);
	}

	/*
	 * XXX Linux magic, helps U30
	 * "PROM sets the IRQ retry value too low, increase it."
	 */
	sc->sc_regs->intr_retry_timer = 0xff;

	/*
	 * attach the pci.. note we pass PCI A tags, etc., for the sabre here.
	 */
	pba.pba_busname = "pci";
#if 0
	pba.pba_flags = sc->sc_psycho_this->pp_flags;
#endif
	pba.pba_dmat = sc->sc_psycho_this->pp_dmat;
	pba.pba_iot = sc->sc_psycho_this->pp_iot;
	pba.pba_memt = sc->sc_psycho_this->pp_memt;

	config_found(self, &pba, psycho_print);
}

int
psycho_print(aux, p)
	void *aux;
	const char *p;
{

	if (p == NULL)
		return (UNCONF);
	return (QUIET);
}

void
psycho_set_intr(sc, ipl, handler, mapper, clearer)
	struct psycho_softc *sc;
	int ipl;
	void *handler;
	u_int64_t *mapper;
	u_int64_t *clearer;
{
	struct intrhand *ih;

	ih = (struct intrhand *)malloc(sizeof(struct intrhand),
	    M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		panic("couldn't malloc intrhand");
	ih->ih_arg = sc;
	ih->ih_map = mapper;
	ih->ih_clr = clearer;
	ih->ih_fun = handler;
	ih->ih_pil = (1<<ipl);
	ih->ih_number = INTVEC(*(ih->ih_map));

	DPRINTF(PDB_INTR, (
	    "; installing handler %p arg %p with number %x pil %u\n",
	    ih->ih_fun, ih->ih_arg, ih->ih_number, ih->ih_pil));

	intr_establish(ipl, ih);
	*(ih->ih_map) |= INTMAP_V;
}

/*
 * PCI bus support
 */

/*
 * allocate a PCI chipset tag and set it's cookie.
 */
pci_chipset_tag_t
psycho_alloc_chipset(pp, node, pc)
	struct psycho_pbm *pp;
	int node;
	pci_chipset_tag_t pc;
{
	pci_chipset_tag_t npc;
	
	npc = malloc(sizeof *npc, M_DEVBUF, M_NOWAIT);
	if (npc == NULL)
		panic("could not allocate pci_chipset_tag_t");
	memcpy(npc, pc, sizeof *pc);
	npc->cookie = pp;
	npc->rootnode = node;
	npc->curnode = node;

	return (npc);
}

/*
 * grovel the OBP for various psycho properties
 */
void
psycho_get_bus_range(node, brp)
	int node;
	int *brp;
{
	int n;

	if (getprop(node, "bus-range", sizeof(*brp), &n, (void **)&brp))
		panic("could not get psycho bus-range");
	if (n != 2)
		panic("broken psycho bus-range");
	DPRINTF(PDB_PROM, ("psycho debug: got `bus-range' for node %08x: %u - %u\n", node, brp[0], brp[1]));
}

void
psycho_get_ranges(node, rp, np)
	int node;
	struct psycho_ranges **rp;
	int *np;
{

	if (getprop(node, "ranges", sizeof(**rp), np, (void **)rp))
		panic("could not get psycho ranges");
	DPRINTF(PDB_PROM, ("psycho debug: got `ranges' for node %08x: %d entries\n", node, *np));
}

/*
 * Interrupt handlers.
 */

int
psycho_ue(arg)
	void *arg;
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	struct psychoreg *regs = sc->sc_regs;
	unsigned long long afsr = regs->psy_ue_afsr;
	unsigned long long afar = regs->psy_ue_afar;

	/*
	 * It's uncorrectable.  Dump the regs and panic.
	 */
	panic("%s: uncorrectable DMA error AFAR %llx (pa=%llx) AFSR %llx\n",
	    sc->sc_dev.dv_xname, afar,
	    (long long)iommu_extract(sc->sc_is, (vaddr_t)afar), afsr);
	return (1);
}

int 
psycho_ce(arg)
	void *arg;
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	struct psychoreg *regs = sc->sc_regs;

	/*
	 * It's correctable.  Dump the regs and continue.
	 */

	printf("%s: correctable DMA error AFAR %llx AFSR %llx\n",
	    sc->sc_dev.dv_xname, 
	    (long long)regs->psy_ce_afar, (long long)regs->psy_ce_afsr);
	return (1);
}

int
psycho_bus_error(sc, bus)
	struct psycho_softc *sc;
	int bus;
{
	struct psychoreg *regs = sc->sc_regs;
	u_int64_t afsr, afar, bits;

	afar = regs->psy_pcictl[bus].pci_afar;
	afsr = regs->psy_pcictl[bus].pci_afsr;

	bits = afsr & (PSY_PCIAFSR_PMA | PSY_PCIAFSR_PTA | PSY_PCIAFSR_PTRY |
	    PSY_PCIAFSR_PPERR | PSY_PCIAFSR_SMA | PSY_PCIAFSR_STA |
	    PSY_PCIAFSR_STRY | PSY_PCIAFSR_SPERR);

	if (bits == 0)
		return (0);

	/*
	 * It's uncorrectable.  Dump the regs and panic.
	 */
	printf("%s: PCI bus %c error AFAR %llx (pa=%llx) AFSR %llx\n",
	    sc->sc_dev.dv_xname, 'A' + bus, (long long)afar,
	    (long long)iommu_extract(sc->sc_is, (vaddr_t)afar),
	    (long long)afsr);

	regs->psy_pcictl[bus].pci_afsr = bits;
	return (1);
}

int 
psycho_bus_a(arg)
	void *arg;
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	return (psycho_bus_error(sc, 0));
}

int 
psycho_bus_b(arg)
	void *arg;
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	return (psycho_bus_error(sc, 1));
}

int 
psycho_powerfail(arg)
	void *arg;
{

	/*
	 * We lost power.  Try to shut down NOW.
	 */
	printf("Power Failure Detected: Shutting down NOW.\n");
	boot(RB_POWERDOWN|RB_HALT);
	return (1);
}

int
psycho_wakeup(arg)
	void *arg;
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	/*
	 * Gee, we don't really have a framework to deal with this
	 * properly.
	 */
	printf("%s: power management wakeup\n",	sc->sc_dev.dv_xname);
	return (1);
}

/*
 * initialise the IOMMU..
 */
void
psycho_iommu_init(sc, tsbsize)
	struct psycho_softc *sc;
	int tsbsize;
{
	char *name;
	struct iommu_state *is = sc->sc_is;
	u_int32_t iobase = -1;
	int *vdma = NULL;
	int nitem;

	/* punch in our copies */
	is->is_bustag = sc->sc_bustag;
	is->is_iommu = &sc->sc_regs->psy_iommu;

	/*
	 * Separate the men from the boys.  Get the `virtual-dma'
	 * property for sabre and use that to make sure the damn
	 * iommu works.
	 *
	 * We could query the `#virtual-dma-size-cells' and
	 * `#virtual-dma-addr-cells' and DTRT, but I'm lazy.
	 */
	if (!getprop(sc->sc_node, "virtual-dma", sizeof(vdma), &nitem, 
		(void **)&vdma)) {
		/* Damn.  Gotta use these values. */
		iobase = vdma[0];
#define	TSBCASE(x)	case 1<<((x)+23): tsbsize = (x); break
		switch (vdma[1]) { 
			TSBCASE(1); TSBCASE(2); TSBCASE(3);
			TSBCASE(4); TSBCASE(5); TSBCASE(6);
		default: 
			printf("bogus tsb size %x, using 7\n", vdma[1]);
			TSBCASE(7);
		}
#undef TSBCASE
	}

	/* give us a nice name.. */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", sc->sc_dev.dv_xname);

	iommu_init(name, is, tsbsize, iobase);
}

/*
 * below here is bus space and bus dma support
 */
bus_space_tag_t
psycho_alloc_bus_tag(pp, type)
	struct psycho_pbm *pp;
	int type;
{
	struct psycho_softc *sc = pp->pp_sc;
	bus_space_tag_t bt;

	bt = (bus_space_tag_t)
		malloc(sizeof(struct sparc_bus_space_tag), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("could not allocate psycho bus tag");

	bzero(bt, sizeof *bt);
	bt->cookie = pp;
	bt->parent = sc->sc_bustag;
	bt->type = type;
	bt->sparc_bus_map = _psycho_bus_map;
	bt->sparc_bus_mmap = psycho_bus_mmap;
	bt->sparc_intr_establish = psycho_intr_establish;
	return (bt);
}

bus_dma_tag_t
psycho_alloc_dma_tag(pp)
	struct psycho_pbm *pp;
{
	struct psycho_softc *sc = pp->pp_sc;
	bus_dma_tag_t dt, pdt = sc->sc_dmatag;

	dt = (bus_dma_tag_t)
		malloc(sizeof(struct sparc_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	if (dt == NULL)
		panic("could not allocate psycho dma tag");

	bzero(dt, sizeof *dt);
	dt->_cookie = pp;
	dt->_parent = pdt;
#define PCOPY(x)	dt->x = pdt->x
	PCOPY(_dmamap_create);
	PCOPY(_dmamap_destroy);
	dt->_dmamap_load = psycho_dmamap_load;
	PCOPY(_dmamap_load_mbuf);
	PCOPY(_dmamap_load_uio);
	dt->_dmamap_load_raw = psycho_dmamap_load_raw;
	dt->_dmamap_unload = psycho_dmamap_unload;
	dt->_dmamap_sync = psycho_dmamap_sync;
	dt->_dmamem_alloc = psycho_dmamem_alloc;
	dt->_dmamem_free = psycho_dmamem_free;
	dt->_dmamem_map = psycho_dmamem_map;
	dt->_dmamem_unmap = psycho_dmamem_unmap;
	PCOPY(_dmamem_mmap);
#undef	PCOPY
	return (dt);
}

/*
 * bus space support.  <sparc64/dev/psychoreg.h> has a discussion about
 * PCI physical addresses.
 */

int
psycho_get_childspace(type)
	int type;
{
	int ss;

	switch (type) {
	case PCI_CONFIG_BUS_SPACE:
		ss = 0x00;
		break;
	case PCI_IO_BUS_SPACE:
		ss = 0x01;
		break;
	case PCI_MEMORY_BUS_SPACE:
		ss = 0x02;
		break;
#if 0
	/* we don't do 64 bit memory space */
	case PCI_MEMORY64_BUS_SPACE:
		ss = 0x03;
		break;
#endif
	default:
		panic("psycho_get_childspace: unknown bus type");
	}

	return (ss);
}

int
_psycho_bus_map(t, btype, offset, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t offset;
	bus_size_t size;
	int	flags;
	vaddr_t vaddr;
	bus_space_handle_t *hp;
{
	struct psycho_pbm *pp = t->cookie;
	struct psycho_softc *sc = pp->pp_sc;
	int i, ss;

	DPRINTF(PDB_BUSMAP, ("_psycho_bus_map: type %d off %qx sz %qx flags %d va %p", t->type, (unsigned long long)offset, (unsigned long long)size, flags,
	    (void *)vaddr));

	ss = psycho_get_childspace(t->type);
	DPRINTF(PDB_BUSMAP, (" cspace %d", ss));

	if (btype == 0)
		btype = t->type;

	for (i = 0; i < pp->pp_nrange; i++) {
		bus_addr_t paddr;

		if (((pp->pp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pp->pp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pp->pp_range[i].phys_hi<<32);
		DPRINTF(PDB_BUSMAP, ("\n_psycho_bus_map: mapping paddr space %lx offset %lx paddr %qx\n",
		    (long)ss, (long)offset,
		    (unsigned long long)paddr));
		return (bus_space_map2(sc->sc_bustag, btype, paddr,
		    size, flags, vaddr, hp));
	}
	DPRINTF(PDB_BUSMAP, (" FAILED\n"));
	return (EINVAL);
}

paddr_t
psycho_bus_mmap(t, paddr, off, prot, flags)
	bus_space_tag_t t;
	bus_addr_t paddr;
	off_t off;
	int prot;
	int flags;
{
	bus_addr_t offset = paddr;
	struct psycho_pbm *pp = t->cookie;
	struct psycho_softc *sc = pp->pp_sc;
	int i, ss;

	ss = psycho_get_childspace(t->type);

	DPRINTF(PDB_BUSMAP, ("_psycho_bus_mmap: prot %d flags %d pa %qx\n",
	    prot, flags, (unsigned long long)paddr));

	for (i = 0; i < pp->pp_nrange; i++) {
		bus_addr_t paddr;

		if (((pp->pp_range[i].cspace >> 24) & 0x03) != ss)
			continue;

		paddr = pp->pp_range[i].phys_lo + offset;
		paddr |= ((bus_addr_t)pp->pp_range[i].phys_hi<<32);
		DPRINTF(PDB_BUSMAP, ("\n_psycho_bus_mmap: mapping paddr "
		    "space %lx offset %lx paddr %qx\n",
		    (long)ss, (long)offset,
		    (unsigned long long)paddr));
		return (bus_space_mmap(sc->sc_bustag, paddr, off, prot, flags));
	}

	return (-1);
}

/*
 * install an interrupt handler for a PCI device
 */
void *
psycho_intr_establish(t, ihandle, level, flags, handler, arg)
	bus_space_tag_t t;
	int ihandle;
	int level;
	int flags;
	int (*handler)(void *);
	void *arg;
{
	struct psycho_pbm *pp = t->cookie;
	struct psycho_softc *sc = pp->pp_sc;
	struct intrhand *ih;
	volatile u_int64_t *intrmapptr = NULL, *intrclrptr = NULL;
	int64_t intrmap = 0;
	int ino;
	long vec = INTVEC(ihandle); 

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	/*
	 * Hunt through all the interrupt mapping regs to look for our
	 * interrupt vector.
	 *
	 * XXX We only compare INOs rather than IGNs since the firmware may
	 * not provide the IGN and the IGN is constant for all device on that
	 * PCI controller.  This could cause problems for the FFB/external
	 * interrupt which has a full vector that can be set arbitrarily.  
	 */


	DPRINTF(PDB_INTR, ("\npsycho_intr_establish: ihandle %x vec %lx", ihandle, vec));
	ino = INTINO(vec);
	DPRINTF(PDB_INTR, (" ino %x", ino));

	/* If the device didn't ask for an IPL, use the one encoded. */
	if (level == IPL_NONE) level = INTLEV(vec);
	/* If it still has no level, print a warning and assign IPL 2 */
	if (level == IPL_NONE) {
		printf("ERROR: no IPL, setting IPL 2.\n");
		level = 2;
	}

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) == 0) {

		DPRINTF(PDB_INTR, ("\npsycho: intr %lx: %p\nHunting for IRQ...\n",
		    (long)ino, intrlev[ino]));

		/* Hunt thru obio first */
		for (intrmapptr = &sc->sc_regs->scsi_int_map,
			 intrclrptr = &sc->sc_regs->scsi_clr_int;
		     intrmapptr <= &sc->sc_regs->ffb1_int_map;
		     intrmapptr++, intrclrptr++) {
			if (INTINO(*intrmapptr) == ino)
				goto found;
		}

		/* Now do PCI interrupts */
		for (intrmapptr = &sc->sc_regs->pcia_slot0_int,
			 intrclrptr = &sc->sc_regs->pcia0_clr_int[0];
		     intrmapptr <= &sc->sc_regs->pcib_slot3_int;
		     intrmapptr++, intrclrptr += 4) {
			if (((*intrmapptr ^ vec) & 0x3c) == 0) {
				intrclrptr += vec & 0x3;
				goto found;
			}
		}
		printf("Cannot find interrupt vector %lx\n", vec);
		return (NULL);

	found:
		/* Register the map and clear intr registers */
		ih->ih_map = intrmapptr;
		ih->ih_clr = intrclrptr;
	}
#ifdef NOT_DEBUG
	if (psycho_debug & PDB_INTR) {
		long i;

		for (i = 0; i < 500000000; i++)
			continue;
	}
#endif

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_pil = level;
	ih->ih_number = ino | sc->sc_ign;

	DPRINTF(PDB_INTR, (
	    "; installing handler %p arg %p with number %x pil %u\n",
	    ih->ih_fun, ih->ih_arg, ih->ih_number, ih->ih_pil));

	intr_establish(ih->ih_pil, ih);

	/*
	 * Enable the interrupt now we have the handler installed.
	 * Read the current value as we can't change it besides the
	 * valid bit so so make sure only this bit is changed.
	 *
	 * XXXX --- we really should use bus_space for this.
	 */
	if (intrmapptr) {
		intrmap = *intrmapptr;
		DPRINTF(PDB_INTR, ("; read intrmap = %016qx",
			(unsigned long long)intrmap));

		/* Enable the interrupt */
		intrmap |= INTMAP_V;
		DPRINTF(PDB_INTR, ("; addr of intrmapptr = %p", intrmapptr));
		DPRINTF(PDB_INTR, ("; writing intrmap = %016qx",
			(unsigned long long)intrmap));
		*intrmapptr = intrmap;
		DPRINTF(PDB_INTR, ("; reread intrmap = %016qx\n",
			(unsigned long long)(intrmap = *intrmapptr)));
	}
	return (ih);
}

/*
 * hooks into the iommu dvma calls.
 */
int
psycho_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	return (iommu_dvmamap_load(t, sc->sc_is, map, buf, buflen, p, flags));
}

void
psycho_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	iommu_dvmamap_unload(t, sc->sc_is, map);
}

int
psycho_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	return (iommu_dvmamap_load_raw(t, sc->sc_is, map, segs, nsegs, flags, size));
}

void
psycho_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

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
psycho_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size;
	bus_size_t alignment;
	bus_size_t boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	return (iommu_dvmamem_alloc(t, sc->sc_is, size, alignment, boundary,
	    segs, nsegs, rsegs, flags));
}

void
psycho_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	iommu_dvmamem_free(t, sc->sc_is, segs, nsegs);
}

int
psycho_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	return (iommu_dvmamem_map(t, sc->sc_is, segs, nsegs, size, kvap, flags));
}

void
psycho_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{
	struct psycho_pbm *pp = (struct psycho_pbm *)t->_cookie;
	struct psycho_softc *sc = pp->pp_sc;

	iommu_dvmamem_unmap(t, sc->sc_is, kva, size);
}

pcireg_t
psycho_pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	struct psycho_pbm *pp = pc->cookie;
	struct psycho_softc *sc = pp->pp_sc;
	pcireg_t val = (pcireg_t)~0;

	DPRINTF(PDB_CONF, ("pci_conf_read: tag %lx reg %x ", 
		(long)tag, reg));
	if (PCITAG_NODE(tag) != -1) {
		DPRINTF(PDB_CONF, ("asi=%x addr=%qx (offset=%x) ...",
			bus_type_asi[sc->sc_configtag->type],
			(long long)(sc->sc_configaddr + 
				PCITAG_OFFSET(tag) + reg),
			(int)PCITAG_OFFSET(tag) + reg));

		val = bus_space_read_4(sc->sc_configtag, sc->sc_configaddr,
			PCITAG_OFFSET(tag) + reg);
	}
#ifdef DEBUG
	else DPRINTF(PDB_CONF, ("pci_conf_read: bogus pcitag %x\n",
	    (int)PCITAG_OFFSET(tag)));
#endif
	DPRINTF(PDB_CONF, (" returning %08x\n", (u_int)val));

	return (val);
}

void
psycho_pci_conf_write(pc, tag, reg, data)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{
	struct psycho_pbm *pp = pc->cookie;
	struct psycho_softc *sc = pp->pp_sc;

	DPRINTF(PDB_CONF, ("pci_conf_write: tag %lx; reg %x; data %x; ", 
		(long)PCITAG_OFFSET(tag), reg, (int)data));
	DPRINTF(PDB_CONF, ("asi = %x; readaddr = %qx (offset = %x)\n",
		bus_type_asi[sc->sc_configtag->type],
		(long long)(sc->sc_configaddr + PCITAG_OFFSET(tag) + reg), 
		(int)PCITAG_OFFSET(tag) + reg));

	/* If we don't know it, just punt. */
	if (PCITAG_NODE(tag) == -1) {
		DPRINTF(PDB_CONF, ("pci_config_write: bad addr"));
		return;
	}

	bus_space_write_4(sc->sc_configtag, sc->sc_configaddr, 
		PCITAG_OFFSET(tag) + reg, data);
}
