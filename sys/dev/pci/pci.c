/*	$OpenBSD: pci.c,v 1.35 2004/12/07 02:11:24 brad Exp $	*/
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

int pcimatch(struct device *, void *, void *);
void pciattach(struct device *, struct device *, void *);

#ifdef USER_PCICONF
struct pci_softc {
	struct device sc_dev;
	pci_chipset_tag_t sc_pc;
	int sc_bus;		/* PCI configuration space bus # */
};
#endif

#ifdef APERTURE
extern int allowaperture;
#endif

struct cfattach pci_ca = {
#ifndef USER_PCICONF
	sizeof(struct device), pcimatch, pciattach
#else
	sizeof(struct pci_softc), pcimatch, pciattach
#endif
};

struct cfdriver pci_cd = {
	NULL, "pci", DV_DULL
};

int	pciprint(void *, const char *);
int	pcisubmatch(struct device *, void *, void *);

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
pcimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
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

	return 1;
}

void
pciattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibus_attach_args *pba = aux;
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, maxndevs, function, nfunctions;
#ifdef USER_PCICONF
	struct pci_softc *sc = (struct pci_softc *)self;
#endif
#ifdef __PCI_BUS_DEVORDER
	char devs[32];
	int i;
#endif
#ifdef __PCI_DEV_FUNCORDER
	char funcs[8];
	int j;
#endif 

	pci_attach_hook(parent, self, pba);
	printf("\n");

	iot = pba->pba_iot;
	memt = pba->pba_memt;
	pc = pba->pba_pc;
	bus = pba->pba_bus;
	maxndevs = pci_bus_maxdevs(pc, bus);

#ifdef USER_PCICONF
	sc->sc_pc = pba->pba_pc;
	sc->sc_bus = bus;
#endif

#ifdef __PCI_BUS_DEVORDER
	pci_bus_devorder(pc, bus, devs);
	for (i = 0; (device = devs[i]) < 32 && device >= 0; i++) {
#else
	for (device = 0; device < maxndevs; device++) {
#endif
		pcitag_t tag;
		pcireg_t id, class, intr;
#ifndef __sparc64__
		pcireg_t bhlcr;
#endif
		struct pci_attach_args pa;
		int pin;

		tag = pci_make_tag(pc, bus, device, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

#ifdef __sparc64__
		pci_dev_funcorder(pc, bus, device, funcs);
		nfunctions = 8;

		/* Invalid vendor ID value or 0 (see below for zero)
		 * ... of course, if the pci_dev_funcorder found
		 *     functions other than zero, we probably want
		 *     to attach them.
		 */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			if (funcs[0] < 0)
				continue;

		for (j = 0; (function = funcs[j]) < nfunctions &&
		    function >= 0; j++) {
#else

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;
		/* XXX Not invalid, but we've done this ~forever. */
		if (PCI_VENDOR(id) == 0)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		nfunctions = PCI_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;

#ifdef __PCI_DEV_FUNCORDER
		pci_dev_funcorder(pc, bus, device, funcs);
		for (j = 0; (function = funcs[j]) < nfunctions &&
		    function >= 0; j++) {
#else
		for (function = 0; function < nfunctions; function++) {
#endif
#endif
			tag = pci_make_tag(pc, bus, device, function);
			id = pci_conf_read(pc, tag, PCI_ID_REG);

			/* Invalid vendor ID value? */
			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
				continue;
			/* XXX Not invalid, but we've done this ~forever. */
			if (PCI_VENDOR(id) == 0)
				continue;

			class = pci_conf_read(pc, tag, PCI_CLASS_REG);
			intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);

			pa.pa_iot = iot;
			pa.pa_memt = memt;
			pa.pa_dmat = pba->pba_dmat;
			pa.pa_pc = pc;
			pa.pa_device = device;
			pa.pa_function = function;
			pa.pa_bus = bus;
			pa.pa_tag = tag;
			pa.pa_id = id;
			pa.pa_class = class;

			/* This is a simplification of the NetBSD code.
			   We don't support turning off I/O or memory
			   on broken hardware. <csapuntz@stanford.edu> */
			pa.pa_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;
#ifdef __i386__
			/*
			 * on i386 we really need to know the device tag
			 * and not the pci bridge tag, in intr_map
			 * to be able to program the device and the
			 * pci interrupt router.
			 */
			pa.pa_intrtag = tag;
			pa.pa_intrswiz = 0;
#else
			if (bus == 0) {
				pa.pa_intrswiz = 0;
				pa.pa_intrtag = tag;
			} else {
				pa.pa_intrswiz = pba->pba_intrswiz + device;
				pa.pa_intrtag = pba->pba_intrtag;
			}
#endif
			pin = PCI_INTERRUPT_PIN(intr);
			pa.pa_rawintrpin = pin;
			if (pin == PCI_INTERRUPT_PIN_NONE) {
				/* no interrupt */
				pa.pa_intrpin = 0;
			} else {
				/*
				 * swizzle it based on the number of
				 * busses we're behind and our device
				 * number.
				 */
				pa.pa_intrpin =			/* XXX */
				    ((pin + pa.pa_intrswiz - 1) % 4) + 1;
			}
			pa.pa_intrline = PCI_INTERRUPT_LINE(intr);

			config_found_sm(self, &pa, pciprint, pcisubmatch);
		}
	}
}

int
pciprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	register struct pci_attach_args *pa = aux;
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
pcisubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;
	int success;

	if (cf->pcicf_dev != PCI_UNK_DEV &&
	    cf->pcicf_dev != pa->pa_device)
		return 0;
	if (cf->pcicf_function != PCI_UNK_FUNCTION &&
	    cf->pcicf_function != pa->pa_function)
		return 0;

	success = (*cf->cf_attach->ca_match)(parent, match, aux);

	/* My Dell BIOS does not enable certain non-critical PCI devices
	   for IO and memory cycles (e.g. network card). This is
	   the generic approach to fixing this problem. Basically, if
	   we support the card, then we enable its IO cycles.
	*/
	if (success) {
		u_int32_t csr = pci_conf_read(pa->pa_pc, pa->pa_tag,
					      PCI_COMMAND_STATUS_REG);

		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			       csr | PCI_COMMAND_MASTER_ENABLE |
			       PCI_COMMAND_IO_ENABLE |
			       PCI_COMMAND_MEM_ENABLE);
	}

	return (success);
}

int
pci_get_capability(pc, tag, capid, offset, value)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int capid;
	int *offset;
	pcireg_t *value;
{
	pcireg_t reg;
	unsigned int ofs;

	reg = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return (0);

	ofs = PCI_CAPLIST_PTR(pci_conf_read(pc, tag, PCI_CAPLISTPTR_REG));
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


int pciopen(dev_t dev, int oflags, int devtype, struct proc *p);
int pciclose(dev_t dev, int flag, int devtype, struct proc *p);
int pciioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);

int
pciopen(dev_t dev, int oflags, int devtype, struct proc *p) 
{
	PCIDEBUG(("pciopen ndevs: %d\n" , pci_cd.cd_ndevs));

#ifndef APERTURE
	if ((oflags & FWRITE) && securelevel > 0) {
		return EPERM;
	}
#else
	if ((oflags & FWRITE) && securelevel > 0 && allowaperture == 0) {
		return EPERM;
	}
#endif
	return 0;
}

int
pciclose(dev_t dev, int flag, int devtype, struct proc *p)
{
	PCIDEBUG(("pciclose\n"));
	return 0;
}

int
pciioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pci_io *io;
	int error;
	pcitag_t tag;
	struct pci_softc *pci;
	pci_chipset_tag_t pc;

	io = (struct pci_io *)data;

	PCIDEBUG(("pciioctl cmd %s", cmd == PCIOCREAD ? "pciocread" 
		  : cmd == PCIOCWRITE ? "pciocwrite" : "unknown"));
	PCIDEBUG(("  bus %d dev %d func %d reg %x\n", io->pi_sel.pc_bus,
		  io->pi_sel.pc_dev, io->pi_sel.pc_func, io->pi_reg));

	if (io->pi_sel.pc_bus >= pci_cd.cd_ndevs) {
		error = ENXIO;
		goto done;
	}
	pci = pci_cd.cd_devs[io->pi_sel.pc_bus];
	if (pci != NULL) {
		pc = pci->sc_pc;
	} else {
		error = ENXIO;
		goto done;
	}
	/* Check bounds */
	if (pci->sc_bus >= 256 || 
	    io->pi_sel.pc_dev >= pci_bus_maxdevs(pc, pci->sc_bus) ||
	    io->pi_sel.pc_func >= 8) {
		error = EINVAL;
		goto done;
	}

	tag = pci_make_tag(pc, pci->sc_bus, io->pi_sel.pc_dev,
			   io->pi_sel.pc_func);

	switch(cmd) {
	case PCIOCGETCONF:
		error = ENODEV;
		break;

	case PCIOCREAD:
		switch(io->pi_width) {
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
		if (!(flag & FWRITE))
			return EPERM;

		switch(io->pi_width) {
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

	default:
		error = ENOTTY;
		break;
	}
 done:
	return (error);
}

#endif
