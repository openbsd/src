/*	$OpenBSD: ppb.c,v 1.29 2009/04/01 19:51:10 kettenis Exp $	*/
/*	$NetBSD: ppb.c,v 1.16 1997/06/06 23:48:05 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/workq.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

struct ppb_softc {
	struct device sc_dev;		/* generic device glue */
	pci_chipset_tag_t sc_pc;	/* our PCI chipset... */
	pcitag_t sc_tag;		/* ...and tag. */
	pci_intr_handle_t sc_ih[4];
	void *sc_intrhand;
	struct device *sc_psc;
	int sc_cap_off;
	struct timeout sc_to;
};

int	ppbmatch(struct device *, void *, void *);
void	ppbattach(struct device *, struct device *, void *);
int	ppbdetach(struct device *self, int flags);

struct cfattach ppb_ca = {
	sizeof(struct ppb_softc), ppbmatch, ppbattach, ppbdetach
};

struct cfdriver ppb_cd = {
	NULL, "ppb", DV_DULL
};

int	ppb_intr(void *);
void	ppb_hotplug_insert(void *, void *);
void	ppb_hotplug_insert_finish(void *);
int	ppb_hotplug_fixup(struct pci_attach_args *);
int	ppb_hotplug_fixup_type0(pci_chipset_tag_t, pcitag_t, pcitag_t);
int	ppb_hotplug_fixup_type1(pci_chipset_tag_t, pcitag_t, pcitag_t);
void	ppb_hotplug_rescan(void *, void *);
void	ppb_hotplug_remove(void *, void *);
int	ppbprint(void *, const char *pnp);

int
ppbmatch(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	/*
	 * This device is mislabeled.  It is not a PCI bridge.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_VIATECH &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_VIATECH_VT82C586_PWR)
		return (0);
	/*
	 * Check the ID register to see that it's a PCI bridge.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_PCI)
		return (1);

	return (0);
}

void
ppbattach(struct device *parent, struct device *self, void *aux)
{
	struct ppb_softc *sc = (struct ppb_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct pcibus_attach_args pba;
	pci_intr_handle_t ih;
	pcireg_t busdata, reg;
	int pin;

	sc->sc_pc = pc;
	sc->sc_tag = pa->pa_tag;

	busdata = pci_conf_read(pc, pa->pa_tag, PPB_REG_BUSINFO);

	if (PPB_BUSINFO_SECONDARY(busdata) == 0) {
		printf(": not configured by system firmware\n");
		return;
	}

#if 0
	/*
	 * XXX can't do this, because we're not given our bus number
	 * (we shouldn't need it), and because we've no way to
	 * decompose our tag.
	 */
	/* sanity check. */
	if (pa->pa_bus != PPB_BUSINFO_PRIMARY(busdata))
		panic("ppbattach: bus in tag (%d) != bus in reg (%d)",
		    pa->pa_bus, PPB_BUSINFO_PRIMARY(busdata));
#endif

	/* Check for PCI Express capabilities and setup hotplug support. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PCIEXPRESS,
	    &sc->sc_cap_off, &reg) && (reg & PCI_PCIE_XCAP_SI)) {
		if (pci_intr_map(pa, &ih) == 0)
			sc->sc_intrhand = pci_intr_establish(pc, ih, IPL_TTY,
			    ppb_intr, sc, self->dv_xname);

		if (sc->sc_intrhand) {
			printf(": %s", pci_intr_string(pc, ih));

			/* Enable hotplug interrupt. */
			reg = pci_conf_read(pc, pa->pa_tag,
			    sc->sc_cap_off + PCI_PCIE_SLCSR);
			reg |= (PCI_PCIE_SLCSR_HPE | PCI_PCIE_SLCSR_PDE);
			pci_conf_write(pc, pa->pa_tag,
			    sc->sc_cap_off + PCI_PCIE_SLCSR, reg);

			timeout_set(&sc->sc_to, ppb_hotplug_insert_finish, sc);
		}
	}

	printf("\n");

	for (pin = PCI_INTERRUPT_PIN_A; pin <= PCI_INTERRUPT_PIN_D; pin++) {
		pa->pa_intrpin = pa->pa_rawintrpin = pin;
		pa->pa_intrline = 0;
		pci_intr_map(pa, &sc->sc_ih[pin - PCI_INTERRUPT_PIN_A]);
	}

	/*
	 * Attach the PCI bus that hangs off of it.
	 *
	 * XXX Don't pass-through Memory Read Multiple.  Should we?
	 * XXX Consult the spec...
	 */
	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = pa->pa_iot;
	pba.pba_memt = pa->pa_memt;
	pba.pba_dmat = pa->pa_dmat;
	pba.pba_pc = pc;
#if 0
	pba.pba_flags = pa->pa_flags & ~PCI_FLAGS_MRM_OKAY;
#endif
	pba.pba_domain = pa->pa_domain;
	pba.pba_bus = PPB_BUSINFO_SECONDARY(busdata);
	pba.pba_bridgeih = sc->sc_ih;
	pba.pba_bridgetag = &sc->sc_tag;
	pba.pba_intrswiz = pa->pa_intrswiz;
	pba.pba_intrtag = pa->pa_intrtag;

	sc->sc_psc = config_found(self, &pba, ppbprint);
}

int
ppbdetach(struct device *self, int flags)
{
	struct ppb_softc *sc = (struct ppb_softc *)self;

	if (sc->sc_intrhand)
		pci_intr_disestablish(sc->sc_pc, sc->sc_intrhand);

	return config_detach_children(self, flags);
}

int
ppb_intr(void *arg)
{
	struct ppb_softc *sc = arg;
	pcireg_t reg;

	/*
	 * XXX ignore hotplug events while in autoconf.  On some
	 * machines with onboard re(4), we gat a bogus hotplug remove
	 * event when we reset that device.  Ignoring that event makes
	 * sure we will not try to forcibly detach re(4) when it isn't
	 * ready to deal with that.
	 */
	if (cold)
		return (0);

	reg = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    sc->sc_cap_off + PCI_PCIE_SLCSR);
	if (reg & PCI_PCIE_SLCSR_PDC) {
		if (reg & PCI_PCIE_SLCSR_PDS)
			workq_add_task(NULL, 0, ppb_hotplug_insert, sc, NULL);
		else
			workq_add_task(NULL, 0, ppb_hotplug_remove, sc, NULL);

		/* Clear interrupts. */
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    sc->sc_cap_off + PCI_PCIE_SLCSR, reg);
		return (1);
	}

	return (0);
}

#ifdef PCI_MACHDEP_ENUMERATE_BUS
#define pci_enumerate_bus PCI_MACHDEP_ENUMERATE_BUS
#else
extern int pci_enumerate_bus(struct pci_softc *,
    int (*)(struct pci_attach_args *), struct pci_attach_args *);
#endif

void
ppb_hotplug_insert(void *arg1, void *arg2)
{
	struct ppb_softc *sc = arg1;
	struct pci_softc *psc = (struct pci_softc *)sc->sc_psc;

	if (!LIST_EMPTY(&psc->sc_devs))
		return;

	/* XXX Powerup the card. */

	/* XXX Turn on LEDs. */

	/* Wait a second for things to settle. */
	timeout_add_sec(&sc->sc_to, 1);
}

void
ppb_hotplug_insert_finish(void *arg)
{
	workq_add_task(NULL, 0, ppb_hotplug_rescan, arg, NULL);
}

int
ppb_hotplug_fixup(struct pci_attach_args *pa)
{
	pcireg_t bhlcr;

	bhlcr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlcr)) {
	case 0:
		return ppb_hotplug_fixup_type0(pa->pa_pc,
		    pa->pa_tag, *pa->pa_bridgetag);
	case 1:
		return ppb_hotplug_fixup_type1(pa->pa_pc,
		    pa->pa_tag, *pa->pa_bridgetag);
	default:
		return (0);
	}
}

int
ppb_hotplug_fixup_type0(pci_chipset_tag_t pc, pcitag_t tag, pcitag_t bridgetag)
{
	pcireg_t blr, type, intr;
	int reg, line;
	bus_addr_t base, io_base, io_limit, mem_base, mem_limit;
	bus_size_t size, io_size, mem_size;

	/*
	 * The code below assumes that the address ranges on our
	 * parent PCI Express bridge are really available and don't
	 * overlap with other devices in the system.
	 */

	/* Figure out the I/O address range of the bridge. */
	blr = pci_conf_read(pc, bridgetag, PPB_REG_IOSTATUS);
	io_base = (blr & 0x000000f0) << 8;
	io_limit = (blr & 0x000f000) | 0x00000fff;
	if (io_limit > io_base)
		io_size = (io_limit - io_base + 1);
	else
		io_size = 0;

	/* Figure out the memory mapped I/O address range of the bridge. */
	blr = pci_conf_read(pc, bridgetag, PPB_REG_MEM);
	mem_base = (blr & 0x0000fff0) << 16;
	mem_limit = (blr & 0xffff0000) | 0x000fffff;
	if (mem_limit > mem_base)
		mem_size = (mem_limit - mem_base + 1);
	else
		mem_size = 0;

	/* Assign resources to the Base Address Registers. */
	for (reg = PCI_MAPREG_START; reg < PCI_MAPREG_END; reg += 4) {
		if (!pci_mapreg_probe(pc, tag, reg, &type))
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, &base, &size, NULL))
			continue;

		if (base != 0)
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			base = roundup(mem_base, size);
			size += base - mem_base;
			if (size > mem_size)
				continue;
			pci_conf_write(pc, tag, reg, base);
			mem_base += size;
			mem_size -= size;
			break;
		case PCI_MAPREG_TYPE_IO:
			base = roundup(io_base, size);
			size += base - io_base;
			if (size > io_size)
				continue;
			pci_conf_write(pc, tag, reg, base);
			io_base += size;
			io_size -= size;
			break;
		default:
			break;
		}

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			reg += 4;
	}

	/*
	 * Fill in the interrupt line for platforms that need it.
	 *
	 * XXX We assume that the interrupt line matches the line used
	 * by the PCI Express bridge.  This may not be true.
	 */
	intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
	if (PCI_INTERRUPT_PIN(intr) != PCI_INTERRUPT_PIN_NONE &&
	    PCI_INTERRUPT_LINE(intr) == 0) {
		/* Get the interrupt line from our parent. */
		intr = pci_conf_read(pc, bridgetag, PCI_INTERRUPT_REG);
		line = PCI_INTERRUPT_LINE(intr);

		intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
		intr &= ~(PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
		intr |= line << PCI_INTERRUPT_LINE_SHIFT;
		pci_conf_write(pc, tag, PCI_INTERRUPT_REG, intr);
	}

	return (0);
}

int
ppb_hotplug_fixup_type1(pci_chipset_tag_t pc, pcitag_t tag, pcitag_t bridgetag)
{
	pcireg_t bhlcr, bir, csr, val;
	int bus, dev, reg;

	bir = pci_conf_read(pc, bridgetag, PPB_REG_BUSINFO);
	if (PPB_BUSINFO_SUBORDINATE(bir) <= PPB_BUSINFO_SECONDARY(bir))
		return (0);

	bus = PPB_BUSINFO_SECONDARY(bir);
	bir = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
	bir &= (0xff << 24);
	bir |= bus++;
	bir |= (bus << 8);
	bir |= (bus << 16);
	pci_conf_write(pc, tag, PPB_REG_BUSINFO, bir);

	for (reg = PPB_REG_IOSTATUS; reg < PPB_REG_BRIDGECONTROL; reg += 4) {
		val = pci_conf_read(pc, bridgetag, reg);
		pci_conf_write(pc, tag, reg, val);
	}

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
	csr |= PCI_COMMAND_MASTER_ENABLE;
	csr |= PCI_COMMAND_INVALIDATE_ENABLE;
	csr |= PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

	for (dev = 0; dev < pci_bus_maxdevs(pc, bus); dev++) {
		tag = pci_make_tag(pc, bus, dev, 0);

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) != 0)
			continue;

		ppb_hotplug_fixup_type0(pc, tag, bridgetag);
	}

	return (0);
}

void
ppb_hotplug_rescan(void *arg1, void *arg2)
{
	struct ppb_softc *sc = arg1;
	struct pci_softc *psc = (struct pci_softc *)sc->sc_psc;

	if (psc) {
		/* Assign resources. */
		pci_enumerate_bus(psc, ppb_hotplug_fixup, NULL);

		/* Attach devices. */
		pci_enumerate_bus(psc, NULL, NULL);
	}
}

void
ppb_hotplug_remove(void *arg1, void *arg2)
{
	struct ppb_softc *sc = arg1;
	struct pci_softc *psc = (struct pci_softc *)sc->sc_psc;

	if (psc)
		pci_detach_devices(psc, DETACH_FORCE);
}

int
ppbprint(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to PPBs; easy. */
	if (pnp)
		printf("pci at %s", pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
