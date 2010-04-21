/*	$OpenBSD: pci.c,v 1.74 2010/04/21 18:55:40 kettenis Exp $	*/
/*	$NetBSD: pci.c,v 1.31 1997/06/06 23:48:04 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI bus autoconfiguration.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

int pcimatch(struct device *, void *, void *);
void pciattach(struct device *, struct device *, void *);
int pcidetach(struct device *, int);
void pcipower(int, void *);
int pciactivate(struct device *, int);

#define NMAPREG			((PCI_MAPREG_END - PCI_MAPREG_START) / \
				    sizeof(pcireg_t))
struct pci_dev {
	LIST_ENTRY(pci_dev) pd_next;
	struct device *pd_dev;
	pcitag_t pd_tag;        /* pci register tag */
	pcireg_t pd_csr;
	pcireg_t pd_bhlc;
	pcireg_t pd_int;
	pcireg_t pd_map[NMAPREG];
};

#ifdef APERTURE
extern int allowaperture;
#endif

struct cfattach pci_ca = {
	sizeof(struct pci_softc), pcimatch, pciattach, pcidetach, pciactivate
};

struct cfdriver pci_cd = {
	NULL, "pci", DV_DULL
};

int	pci_ndomains;

struct proc *pci_vga_proc;
struct pci_softc *pci_vga_pci;
pcitag_t pci_vga_tag;
int	pci_vga_count;

int	pciprint(void *, const char *);
int	pcisubmatch(struct device *, void *, void *);

#ifdef PCI_MACHDEP_ENUMERATE_BUS
#define pci_enumerate_bus PCI_MACHDEP_ENUMERATE_BUS
#else
int pci_enumerate_bus(struct pci_softc *,
    int (*)(struct pci_attach_args *), struct pci_attach_args *);
#endif
int	pci_reserve_resources(struct pci_attach_args *);
int	pci_count_vga(struct pci_attach_args *);
int	pci_primary_vga(struct pci_attach_args *);

/*
 * Important note about PCI-ISA bridges:
 *
 * Callbacks are used to configure these devices so that ISA/EISA bridges
 * can attach their child busses after PCI configuration is done.
 *
 * This works because:
 *	(1) there can be at most one ISA/EISA bridge per PCI bus, and
 *	(2) any ISA/EISA bridges must be attached to primary PCI
 *	    busses (i.e. bus zero).
 *
 * That boils down to: there can only be one of these outstanding
 * at a time, it is cleared when configuring PCI bus 0 before any
 * subdevices have been found, and it is run after all subdevices
 * of PCI bus 0 have been found.
 *
 * This is needed because there are some (legacy) PCI devices which
 * can show up as ISA/EISA devices as well (the prime example of which
 * are VGA controllers).  If you attach ISA from a PCI-ISA/EISA bridge,
 * and the bridge is seen before the video board is, the board can show
 * up as an ISA device, and that can (bogusly) complicate the PCI device's
 * attach code, or make the PCI device not be properly attached at all.
 *
 * We use the generic config_defer() facility to achieve this.
 */

int
pcimatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct pcibus_attach_args *pba = aux;

	if (strcmp(pba->pba_busname, cf->cf_driver->cd_name))
		return (0);

	/* Check the locators */
	if (cf->pcibuscf_bus != PCIBUS_UNK_BUS &&
	    cf->pcibuscf_bus != pba->pba_bus)
		return (0);

	/* sanity */
	if (pba->pba_bus < 0 || pba->pba_bus > 255)
		return (0);

	/*
	 * XXX check other (hardware?) indicators
	 */

	return (1);
}

void
pciattach(struct device *parent, struct device *self, void *aux)
{
	struct pcibus_attach_args *pba = aux;
	struct pci_softc *sc = (struct pci_softc *)self;

	pci_attach_hook(parent, self, pba);

	printf("\n");

	LIST_INIT(&sc->sc_devs);
	sc->sc_powerhook = powerhook_establish(pcipower, sc);

	sc->sc_iot = pba->pba_iot;
	sc->sc_memt = pba->pba_memt;
	sc->sc_dmat = pba->pba_dmat;
	sc->sc_pc = pba->pba_pc;
	sc->sc_ioex = pba->pba_ioex;
	sc->sc_memex = pba->pba_memex;
	sc->sc_pmemex = pba->pba_pmemex;
	sc->sc_domain = pba->pba_domain;
	sc->sc_bus = pba->pba_bus;
	sc->sc_bridgetag = pba->pba_bridgetag;
	sc->sc_bridgeih = pba->pba_bridgeih;
	sc->sc_maxndevs = pci_bus_maxdevs(pba->pba_pc, pba->pba_bus);
	sc->sc_intrswiz = pba->pba_intrswiz;
	sc->sc_intrtag = pba->pba_intrtag;
	pci_enumerate_bus(sc, pci_reserve_resources, NULL);
	pci_enumerate_bus(sc, pci_count_vga, NULL);
	if (pci_enumerate_bus(sc, pci_primary_vga, NULL))
		pci_vga_pci = sc;
	pci_enumerate_bus(sc, NULL, NULL);
}

int
pcidetach(struct device *self, int flags)
{
	return pci_detach_devices((struct pci_softc *)self, flags);
}

int
pciactivate(struct device *self, int act)
{
	int rv = 0;

	switch (act) {
	case DVACT_SUSPEND:
		rv = config_activate_children(self, act);
		pcipower(PWR_SUSPEND, self);	
		break;
	case DVACT_RESUME:
		pcipower(PWR_RESUME, self);
		rv = config_activate_children(self, act);
		break;
	}
	return (rv);
}

/* save and restore the pci config space */
void
pcipower(int why, void *arg)
{
	struct pci_softc *sc = (struct pci_softc *)arg;
	struct pci_dev *pd;
	pcireg_t reg;
	int i;

	LIST_FOREACH(pd, &sc->sc_devs, pd_next) {
		if (why != PWR_RESUME) {
			for (i = 0; i < NMAPREG; i++)
			       pd->pd_map[i] = pci_conf_read(sc->sc_pc,
				   pd->pd_tag, PCI_MAPREG_START + (i * 4));
			pd->pd_csr = pci_conf_read(sc->sc_pc, pd->pd_tag,
			   PCI_COMMAND_STATUS_REG);
			pd->pd_bhlc = pci_conf_read(sc->sc_pc, pd->pd_tag,
			   PCI_BHLC_REG);
			pd->pd_int = pci_conf_read(sc->sc_pc, pd->pd_tag,
			   PCI_INTERRUPT_REG);
		} else {
			for (i = 0; i < NMAPREG; i++)
				pci_conf_write(sc->sc_pc, pd->pd_tag,
				    PCI_MAPREG_START + (i * 4),
					pd->pd_map[i]);
			reg = pci_conf_read(sc->sc_pc, pd->pd_tag,
			    PCI_COMMAND_STATUS_REG);
			pci_conf_write(sc->sc_pc, pd->pd_tag,
			    PCI_COMMAND_STATUS_REG,
			    (reg & 0xffff0000) | (pd->pd_csr & 0x0000ffff));
			pci_conf_write(sc->sc_pc, pd->pd_tag, PCI_BHLC_REG,
			    pd->pd_bhlc);
			pci_conf_write(sc->sc_pc, pd->pd_tag, PCI_INTERRUPT_REG,
			    pd->pd_int);
		}
	}
}

int
pciprint(void *aux, const char *pnp)
{
	struct pci_attach_args *pa = aux;
	char devinfo[256];

	if (pnp) {
		pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo,
		    sizeof devinfo);
		printf("%s at %s", devinfo, pnp);
	}
	printf(" dev %d function %d", pa->pa_device, pa->pa_function);
	if (!pnp) {
		pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo,
		    sizeof devinfo);
		printf(" %s", devinfo);
	}

	return (UNCONF);
}

int
pcisubmatch(struct device *parent, void *match,  void *aux)
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (cf->pcicf_dev != PCI_UNK_DEV &&
	    cf->pcicf_dev != pa->pa_device)
		return (0);
	if (cf->pcicf_function != PCI_UNK_FUNCTION &&
	    cf->pcicf_function != pa->pa_function)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

int
pci_probe_device(struct pci_softc *sc, pcitag_t tag,
    int (*match)(struct pci_attach_args *), struct pci_attach_args *pap)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	struct pci_attach_args pa;
	struct pci_dev *pd;
	struct device *dev;
	pcireg_t id, csr, class, intr, bhlcr;
	int ret = 0, pin, bus, device, function;

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlcr) > 2)
		return (0);

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	class = pci_conf_read(pc, tag, PCI_CLASS_REG);

	/* Invalid vendor ID value? */
	if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
		return (0);
	/* XXX Not invalid, but we've done this ~forever. */
	if (PCI_VENDOR(id) == 0)
		return (0);

	pa.pa_iot = sc->sc_iot;
	pa.pa_memt = sc->sc_memt;
	pa.pa_dmat = sc->sc_dmat;
	pa.pa_pc = pc;
	pa.pa_ioex = sc->sc_ioex;
	pa.pa_memex = sc->sc_memex;
	pa.pa_pmemex = sc->sc_pmemex;
	pa.pa_domain = sc->sc_domain;
	pa.pa_bus = bus;
	pa.pa_device = device;
	pa.pa_function = function;
	pa.pa_tag = tag;
	pa.pa_id = id;
	pa.pa_class = class;
	pa.pa_bridgetag = sc->sc_bridgetag;
	pa.pa_bridgeih = sc->sc_bridgeih;

	/* This is a simplification of the NetBSD code.
	   We don't support turning off I/O or memory
	   on broken hardware. <csapuntz@stanford.edu> */
	pa.pa_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;

	if (sc->sc_bridgetag == NULL) {
		pa.pa_intrswiz = 0;
		pa.pa_intrtag = tag;
	} else {
		pa.pa_intrswiz = sc->sc_intrswiz + device;
		pa.pa_intrtag = sc->sc_intrtag;
	}

	intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);

	pin = PCI_INTERRUPT_PIN(intr);
	pa.pa_rawintrpin = pin;
	if (pin == PCI_INTERRUPT_PIN_NONE) {
		/* no interrupt */
		pa.pa_intrpin = 0;
	} else {
		/*
		 * swizzle it based on the number of busses we're
		 * behind and our device number.
		 */
		pa.pa_intrpin = 	/* XXX */
		    ((pin + pa.pa_intrswiz - 1) % 4) + 1;
	}
	pa.pa_intrline = PCI_INTERRUPT_LINE(intr);

	if (match != NULL) {
		ret = (*match)(&pa);
		if (ret != 0 && pap != NULL)
			*pap = pa;
	} else {
		if ((dev = config_found_sm(&sc->sc_dev, &pa, pciprint,
		    pcisubmatch))) {
			pcireg_t reg;

			/* skip header type != 0 */
			reg = pci_conf_read(pc, tag, PCI_BHLC_REG);
			if (PCI_HDRTYPE_TYPE(reg) != 0)
				return(0);
			if (pci_get_capability(pc, tag,
			    PCI_CAP_PWRMGMT, NULL, NULL) == 0)
				return(0);
			if (!(pd = malloc(sizeof *pd, M_DEVBUF,
			    M_NOWAIT)))
				return(0);
			pd->pd_tag = tag;
			pd->pd_dev = dev;
			LIST_INSERT_HEAD(&sc->sc_devs, pd, pd_next);
		}
	}

	return (ret);
}

int
pci_detach_devices(struct pci_softc *sc, int flags)
{
	struct pci_dev *pd, *next;
	int ret;

	ret = config_detach_children(&sc->sc_dev, flags);
	if (ret != 0)
		return (ret);

	for (pd = LIST_FIRST(&sc->sc_devs);
	     pd != LIST_END(&sc->sc_devs); pd = next) {
		next = LIST_NEXT(pd, pd_next);
		free(pd, M_DEVBUF);
	}
	LIST_INIT(&sc->sc_devs);

	return (0);
}

int
pci_get_capability(pci_chipset_tag_t pc, pcitag_t tag, int capid,
    int *offset, pcireg_t *value)
{
	pcireg_t reg;
	unsigned int ofs;

	reg = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return (0);

	/* Determine the Capability List Pointer register to start with. */
	reg = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(reg)) {
	case 0:	/* standard device header */
	case 1: /* PCI-PCI bridge header */
		ofs = PCI_CAPLISTPTR_REG;
		break;
	case 2:	/* PCI-CardBus bridge header */
		ofs = PCI_CARDBUS_CAPLISTPTR_REG;
		break;
	default:
		return (0);
	}

	ofs = PCI_CAPLIST_PTR(pci_conf_read(pc, tag, ofs));
	while (ofs != 0) {
#ifdef DIAGNOSTIC
		if ((ofs & 3) || (ofs < 0x40))
			panic("pci_get_capability");
#endif
		reg = pci_conf_read(pc, tag, ofs);
		if (PCI_CAPLIST_CAP(reg) == capid) {
			if (offset)
				*offset = ofs;
			if (value)
				*value = reg;
			return (1);
		}
		ofs = PCI_CAPLIST_NEXT(reg);
	}

	return (0);
}

int
pci_find_device(struct pci_attach_args *pa,
		int (*match)(struct pci_attach_args *))
{
	extern struct cfdriver pci_cd;
	struct device *pcidev;
	int i;

	for (i = 0; i < pci_cd.cd_ndevs; i++) {
		pcidev = pci_cd.cd_devs[i];
		if (pcidev != NULL &&
		    pci_enumerate_bus((struct pci_softc *)pcidev,
		    		      match, pa) != 0)
			return (1);
	}
	return (0);
}

int
pci_set_powerstate(pci_chipset_tag_t pc, pcitag_t tag, int state)
{
	pcireg_t reg;
	int offset;

	if (pci_get_capability(pc, tag, PCI_CAP_PWRMGMT, &offset, 0)) {
		reg = pci_conf_read(pc, tag, offset + PCI_PMCSR);
		if ((reg & PCI_PMCSR_STATE_MASK) != state) {
			pci_conf_write(pc, tag, offset + PCI_PMCSR,
			    (reg & ~PCI_PMCSR_STATE_MASK) | state);
			return (reg & PCI_PMCSR_STATE_MASK);
		}
	}
	return (state);
}

#ifndef PCI_MACHDEP_ENUMERATE_BUS
/*
 * Generic PCI bus enumeration routine.  Used unless machine-dependent
 * code needs to provide something else.
 */
int
pci_enumerate_bus(struct pci_softc *sc,
    int (*match)(struct pci_attach_args *), struct pci_attach_args *pap)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	int device, function, nfunctions, ret;
	const struct pci_quirkdata *qd;
	pcireg_t id, bhlcr;
	pcitag_t tag;

	for (device = 0; device < sc->sc_maxndevs; device++) {
		tag = pci_make_tag(pc, sc->sc_bus, device, 0);

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) > 2)
			continue;

		id = pci_conf_read(pc, tag, PCI_ID_REG);

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;
		/* XXX Not invalid, but we've done this ~forever. */
		if (PCI_VENDOR(id) == 0)
			continue;

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));

		if (qd != NULL &&
		      (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0)
			nfunctions = 8;
		else if (qd != NULL &&
		      (qd->quirks & PCI_QUIRK_MONOFUNCTION) != 0)
			nfunctions = 1;
		else
			nfunctions = PCI_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;

		for (function = 0; function < nfunctions; function++) {
			tag = pci_make_tag(pc, sc->sc_bus, device, function);
			ret = pci_probe_device(sc, tag, match, pap);
			if (match != NULL && ret != 0)
				return (ret);
		}
 	}

	return (0);
}
#endif /* PCI_MACHDEP_ENUMERATE_BUS */

int
pci_reserve_resources(struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t bhlc, blr, type;
	bus_addr_t base, limit;
	bus_size_t size;
	int reg, reg_start, reg_end;
	int flags;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlc)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		break;
	case 1: /* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		break;
	case 2: /* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		break;
	default:
		return (0);
	}
    
	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (!pci_mapreg_probe(pc, tag, reg, &type))
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, &base, &size, &flags))
			continue;

		if (base == 0)
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
#ifdef BUS_SPACE_MAP_PREFETCHABLE
			if (ISSET(flags, BUS_SPACE_MAP_PREFETCHABLE) &&
			    pa->pa_pmemex && extent_alloc_region(pa->pa_pmemex,
			    base, size, EX_NOWAIT) == 0) {
				break;
			}
#endif
			if (pa->pa_memex && extent_alloc_region(pa->pa_memex,
			    base, size, EX_NOWAIT)) {
				printf("mem address conflict 0x%x/0x%x\n",
				    base, size);
				pci_conf_write(pc, tag, reg, 0);
				if (type & PCI_MAPREG_MEM_TYPE_64BIT)
					pci_conf_write(pc, tag, reg + 4, 0);
			}
			break;
		case PCI_MAPREG_TYPE_IO:
			if (pa->pa_ioex && extent_alloc_region(pa->pa_ioex,
			    base, size, EX_NOWAIT)) {
				printf("io address conflict 0x%x/0x%x\n",
				    base, size);
				pci_conf_write(pc, tag, reg, 0);
			}
			break;
		}

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			reg += 4;
	}

	if (PCI_HDRTYPE_TYPE(bhlc) != 1)
		return (0);

	/* Figure out the I/O address range of the bridge. */
	blr = pci_conf_read(pc, tag, PPB_REG_IOSTATUS);
	base = (blr & 0x000000f0) << 8;
	limit = (blr & 0x000f000) | 0x00000fff;
	blr = pci_conf_read(pc, tag, PPB_REG_IO_HI);
	base |= (blr & 0x0000ffff) << 16;
	limit |= (blr & 0xffff0000);
	if (limit > base)
		size = (limit - base + 1);
	else
		size = 0;
	if (pa->pa_ioex && base > 0 && size > 0) {
		if (extent_alloc_region(pa->pa_ioex, base, size, EX_NOWAIT)) {
			printf("bridge io address conflict 0x%x/0x%x\n",
			       base, size);
			blr &= 0xffff0000;
			blr |= 0x000000f0;
			pci_conf_write(pc, tag, PPB_REG_IOSTATUS, blr);
		}
	}

	/* Figure out the memory mapped I/O address range of the bridge. */
	blr = pci_conf_read(pc, tag, PPB_REG_MEM);
	base = (blr & 0x0000fff0) << 16;
	limit = (blr & 0xfff00000) | 0x000fffff;
	if (limit > base)
		size = (limit - base + 1);
	else
		size = 0;
	if (pa->pa_memex && base > 0 && size > 0) {
		if (extent_alloc_region(pa->pa_memex, base, size, EX_NOWAIT)) {
			printf("bridge mem address conflict 0x%x/0x%x\n",
			       base, size);
			pci_conf_write(pc, tag, PPB_REG_MEM, 0x0000fff0);
		}
	}

	/* Figure out the prefetchable memory address range of the bridge. */
	blr = pci_conf_read(pc, tag, PPB_REG_PREFMEM);
	base = (blr & 0x0000fff0) << 16;
	limit = (blr & 0xfff00000) | 0x000fffff;
	if (limit > base)
		size = (limit - base + 1);
	else
		size = 0;
	if (pa->pa_pmemex && base > 0 && size > 0) {
		if (extent_alloc_region(pa->pa_pmemex, base, size, EX_NOWAIT)) {
			printf("bridge mem address conflict 0x%x/0x%x\n",
			       base, size);
			pci_conf_write(pc, tag, PPB_REG_PREFMEM, 0x0000fff0);
		}
	} else if (pa->pa_memex && base > 0 && size > 0) {
		if (extent_alloc_region(pa->pa_memex, base, size, EX_NOWAIT)) {
			printf("bridge mem address conflict 0x%x/0x%x\n",
			       base, size);
			pci_conf_write(pc, tag, PPB_REG_PREFMEM, 0x0000fff0);
		}
	}

	return (0);
}

/*
 * Vital Product Data (PCI 2.2)
 */

int
pci_vpd_read(pci_chipset_tag_t pc, pcitag_t tag, int offset, int count,
    pcireg_t *data)
{
	uint32_t reg;
	int ofs, i, j;

	KASSERT(data != NULL);
	KASSERT((offset + count) < 0x7fff);

	if (pci_get_capability(pc, tag, PCI_CAP_VPD, &ofs, &reg) == 0)
		return (1);

	for (i = 0; i < count; offset += sizeof(*data), i++) {
		reg &= 0x0000ffff;
		reg &= ~PCI_VPD_OPFLAG;
		reg |= PCI_VPD_ADDRESS(offset);
		pci_conf_write(pc, tag, ofs, reg);

		/*
		 * PCI 2.2 does not specify how long we should poll
		 * for completion nor whether the operation can fail.
		 */
		j = 0;
		do {
			if (j++ == 20)
				return (1);
			delay(4);
			reg = pci_conf_read(pc, tag, ofs);
		} while ((reg & PCI_VPD_OPFLAG) == 0);
		data[i] = pci_conf_read(pc, tag, PCI_VPD_DATAREG(ofs));
	}

	return (0);
}

int
pci_vpd_write(pci_chipset_tag_t pc, pcitag_t tag, int offset, int count,
    pcireg_t *data)
{
	pcireg_t reg;
	int ofs, i, j;

	KASSERT(data != NULL);
	KASSERT((offset + count) < 0x7fff);

	if (pci_get_capability(pc, tag, PCI_CAP_VPD, &ofs, &reg) == 0)
		return (1);

	for (i = 0; i < count; offset += sizeof(*data), i++) {
		pci_conf_write(pc, tag, PCI_VPD_DATAREG(ofs), data[i]);

		reg &= 0x0000ffff;
		reg |= PCI_VPD_OPFLAG;
		reg |= PCI_VPD_ADDRESS(offset);
		pci_conf_write(pc, tag, ofs, reg);

		/*
		 * PCI 2.2 does not specify how long we should poll
		 * for completion nor whether the operation can fail.
		 */
		j = 0;
		do {
			if (j++ == 20)
				return (1);
			delay(1);
			reg = pci_conf_read(pc, tag, ofs);
		} while (reg & PCI_VPD_OPFLAG);
	}

	return (0);
}

int
pci_matchbyid(struct pci_attach_args *pa, const struct pci_matchid *ids,
    int nent)
{
	const struct pci_matchid *pm;
	int i;

	for (i = 0, pm = ids; i < nent; i++, pm++)
		if (PCI_VENDOR(pa->pa_id) == pm->pm_vid &&
		    PCI_PRODUCT(pa->pa_id) == pm->pm_pid)
			return (1);
	return (0);
}

#ifdef USER_PCICONF
/*
 * This is the user interface to PCI configuration space.
 */
  
#include <sys/pciio.h>
#include <sys/fcntl.h>

#ifdef DEBUG
#define PCIDEBUG(x) printf x
#else
#define PCIDEBUG(x)
#endif

void pci_disable_vga(pci_chipset_tag_t, pcitag_t);
void pci_enable_vga(pci_chipset_tag_t, pcitag_t);
void pci_route_vga(struct pci_softc *);
void pci_unroute_vga(struct pci_softc *);

int pciopen(dev_t dev, int oflags, int devtype, struct proc *p);
int pciclose(dev_t dev, int flag, int devtype, struct proc *p);
int pciioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);

int
pciopen(dev_t dev, int oflags, int devtype, struct proc *p) 
{
	PCIDEBUG(("pciopen ndevs: %d\n" , pci_cd.cd_ndevs));

	if (minor(dev) >= pci_ndomains) {
		return ENXIO;
	}

#ifndef APERTURE
	if ((oflags & FWRITE) && securelevel > 0) {
		return EPERM;
	}
#else
	if ((oflags & FWRITE) && securelevel > 0 && allowaperture == 0) {
		return EPERM;
	}
#endif
	return (0);
}

int
pciclose(dev_t dev, int flag, int devtype, struct proc *p)
{
	PCIDEBUG(("pciclose\n"));

	pci_vga_proc = NULL;
	return (0);
}

int
pciioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pcisel *sel = (struct pcisel *)data;
	struct pci_io *io;
	struct pci_rom *rom;
	int i, error;
	pcitag_t tag;
	struct pci_softc *pci;
	pci_chipset_tag_t pc;

	switch (cmd) {
	case PCIOCREAD:
		break;
	case PCIOCWRITE:
		if (!(flag & FWRITE))
			return EPERM;
		break;
	case PCIOCGETROMLEN:
	case PCIOCGETROM:
		break;
	case PCIOCGETVGA:
	case PCIOCSETVGA:
		if (pci_vga_pci == NULL)
			return EINVAL;
		break;
	default:
		return ENOTTY;
	}

	for (i = 0; i < pci_cd.cd_ndevs; i++) {
		pci = pci_cd.cd_devs[i];
		if (pci != NULL && pci->sc_domain == minor(dev) &&
		    pci->sc_bus == sel->pc_bus)
			break;
	}
	if (i >= pci_cd.cd_ndevs)
		return ENXIO;

	/* Check bounds */
	if (pci->sc_bus >= 256 || 
	    sel->pc_dev >= pci_bus_maxdevs(pci->sc_pc, pci->sc_bus) ||
	    sel->pc_func >= 8)
		return EINVAL;

	pc = pci->sc_pc;
	tag = pci_make_tag(pc, sel->pc_bus, sel->pc_dev, sel->pc_func);

	switch (cmd) {
	case PCIOCREAD:
		io = (struct pci_io *)data;
		switch (io->pi_width) {
		case 4:
			/* Make sure the register is properly aligned */
			if (io->pi_reg & 0x3) 
				return EINVAL;
			io->pi_data = pci_conf_read(pc, tag, io->pi_reg);
			error = 0;
			break;
		default:
			error = ENODEV;
			break;
		}
		break;

	case PCIOCWRITE:
		io = (struct pci_io *)data;
		switch (io->pi_width) {
		case 4:
			/* Make sure the register is properly aligned */
			if (io->pi_reg & 0x3)
				return EINVAL;
			pci_conf_write(pc, tag, io->pi_reg, io->pi_data);
			error = 0;
			break;
		default:
			error = ENODEV;
			break;
		}
		break;

	case PCIOCGETROMLEN:
	case PCIOCGETROM:
	{
		pcireg_t addr, mask, bhlc;
		bus_space_handle_t h;
		bus_size_t len, off;
		char buf[256];
		int s;

		rom = (struct pci_rom *)data;

		bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlc) != 0)
			return (ENODEV);

		s = splhigh();
		addr = pci_conf_read(pc, tag, PCI_ROM_REG);
		pci_conf_write(pc, tag, PCI_ROM_REG, ~PCI_ROM_ENABLE);
		mask = pci_conf_read(pc, tag, PCI_ROM_REG);
		pci_conf_write(pc, tag, PCI_ROM_REG, addr);
		splx(s);

		/*
		 * Section 6.2.5.2 `Expansion ROM Base Addres Register',
		 *
		 * tells us that only the upper 21 bits are writable.
		 * This means that the size of a ROM must be a
		 * multiple of 2 KB.  So reading the ROM in chunks of
		 * 256 bytes should work just fine.
		 */
		if ((PCI_ROM_ADDR(addr) == 0 ||
		     PCI_ROM_SIZE(mask) % sizeof(buf)) != 0)
			return (ENODEV);

		/* If we're just after the size, skip reading the ROM. */
		if (cmd == PCIOCGETROMLEN) {
			error = 0;
			goto fail;
		}

		if (rom->pr_romlen < PCI_ROM_SIZE(mask)) {
			error = ENOMEM;
			goto fail;
		}

		error = bus_space_map(pci->sc_memt, PCI_ROM_ADDR(addr),
		    PCI_ROM_SIZE(mask), 0, &h);
		if (error)
			goto fail;

		off = 0;
		len = PCI_ROM_SIZE(mask);
		while (len > 0 && error == 0) {
			s = splhigh();
			pci_conf_write(pc, tag, PCI_ROM_REG,
			    addr | PCI_ROM_ENABLE);
			bus_space_read_region_1(pci->sc_memt, h, off,
			    buf, sizeof(buf));
			pci_conf_write(pc, tag, PCI_ROM_REG, addr);
			splx(s);

			error = copyout(buf, rom->pr_rom + off, sizeof(buf));
			off += sizeof(buf);
			len -= sizeof(buf);
		}

		bus_space_unmap(pci->sc_memt, h, PCI_ROM_SIZE(mask));

	fail:
		rom->pr_romlen = PCI_ROM_SIZE(mask);
		break;
	}

	case PCIOCGETVGA:
	{
		struct pci_vga *vga = (struct pci_vga *)data;
		int bus, device, function;

		pci_decompose_tag(pci_vga_pci->sc_pc, pci_vga_tag,
		    &bus, &device, &function);
		vga->pv_sel.pc_bus = bus;
		vga->pv_sel.pc_dev = device;
		vga->pv_sel.pc_func = function;
		error = 0;
		break;
	}
	case PCIOCSETVGA:
	{
		struct pci_vga *vga = (struct pci_vga *)data;

		switch (vga->pv_lock) {
		case PCI_VGA_UNLOCK:
		case PCI_VGA_LOCK:
		case PCI_VGA_TRYLOCK:
			break;
		default:
			return (EINVAL);
		}

		if (vga->pv_lock == PCI_VGA_UNLOCK) {
			if (pci_vga_proc != p)
				return (EINVAL);
			pci_vga_proc = NULL;
			wakeup(&pci_vga_proc);
			return (0);
		}

		while (pci_vga_proc != p && pci_vga_proc != NULL) {
			if (vga->pv_lock == PCI_VGA_TRYLOCK)
				return (EBUSY);
			error = tsleep(&pci_vga_proc, PLOCK | PCATCH,
			    "vgalk", 0);
			if (error)
				return (error);
		}
		pci_vga_proc = p;

		if (tag != pci_vga_tag) {
			pci_disable_vga(pci_vga_pci->sc_pc, pci_vga_tag);
			if (pci != pci_vga_pci) {
				pci_unroute_vga(pci_vga_pci);
				pci_route_vga(pci);
				pci_vga_pci = pci;
			}
			pci_enable_vga(pc, tag);
			pci_vga_tag = tag;
		}

		error = 0;
		break;
	}

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

#endif

void
pci_disable_vga(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t csr;

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
}

void
pci_enable_vga(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t csr;

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
}

void
pci_route_vga(struct pci_softc *sc)
{
	pci_chipset_tag_t pc;
	pcireg_t bc;

	if (sc->sc_bridgetag == NULL)
		return;

	bc = pci_conf_read(pc, *sc->sc_bridgetag, PPB_REG_BRIDGECONTROL);
	bc |= PPB_BC_VGA_ENABLE;
	pci_conf_write(pc, *sc->sc_bridgetag, PPB_REG_BRIDGECONTROL, bc);

	pci_route_vga((struct pci_softc *)sc->sc_dev.dv_parent->dv_parent);
}

void
pci_unroute_vga(struct pci_softc *sc)
{
	pci_chipset_tag_t pc;
	pcireg_t bc;

	if (sc->sc_bridgetag == NULL)
		return;

	bc = pci_conf_read(pc, *sc->sc_bridgetag, PPB_REG_BRIDGECONTROL);
	bc &= ~PPB_BC_VGA_ENABLE;
	pci_conf_write(pc, *sc->sc_bridgetag, PPB_REG_BRIDGECONTROL, bc);

	pci_unroute_vga((struct pci_softc *)sc->sc_dev.dv_parent->dv_parent);
}

int
pci_count_vga(struct pci_attach_args *pa)
{
	/* XXX For now, only handle the first PCI domain. */
	if (pa->pa_domain != 0)
		return (0);

	if ((PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA) &&
	    (PCI_CLASS(pa->pa_class) != PCI_CLASS_PREHISTORIC ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_PREHISTORIC_VGA))
		return (0);

	pci_vga_count++;

	return (0);
}

int
pci_primary_vga(struct pci_attach_args *pa)
{
	/* XXX For now, only handle the first PCI domain. */
	if (pa->pa_domain != 0)
		return (0);

	if ((PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA) &&
	    (PCI_CLASS(pa->pa_class) != PCI_CLASS_PREHISTORIC ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_PREHISTORIC_VGA))
		return (0);

	if ((pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    != (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
		return (0);

	pci_vga_tag = pa->pa_tag;

	return (1);
}
