/* $OpenBSD: vga_pci.c,v 1.11 2002/03/14 01:27:00 millert Exp $ */
/* $NetBSD: vga_pci.c,v 1.3 1998/06/08 06:55:58 thorpej Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "vga.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

struct vga_pci_softc {
	struct device sc_dev; 
 
	pcitag_t sc_pcitag;		/* PCI tag, in case we need it. */
#if 0
	struct vga_config *sc_vc;	/* VGA configuration */
#endif
};

int	vga_pci_match(struct device *, void *, void *);
void	vga_pci_attach(struct device *, struct device *, void *);

struct cfattach vga_pci_ca = {
	sizeof(struct vga_pci_softc), vga_pci_match, vga_pci_attach,
};

int
vga_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int potential;

	potential = 0;

	/*
	 * If it's prehistoric/vga or display/vga, we might match.
	 * For the console device, this is jut a sanity check.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		potential = 1;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		potential = 1;

	if (!potential)
		return (0);

	/* check whether it is disabled by firmware */
	if ((pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    != (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
		return (0);

	/* If it's the console, we have a winner! */
	if (vga_is_console(pa->pa_iot, WSDISPLAY_TYPE_PCIVGA))
		return (1);

	/*
	 * If we might match, make sure that the card actually looks OK.
	 */
	if (!vga_common_probe(pa->pa_iot, pa->pa_memt))
		return (0);

	return (1);
}

void
vga_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;
	char devinfo[256];

	sc->sc_pcitag = pa->pa_tag;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf("\n");

	vga_common_attach(self, pa->pa_iot, pa->pa_memt,
			  WSDISPLAY_TYPE_PCIVGA);
}

int
vga_pci_cnattach(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	return (vga_cnattach(iot, memt, WSDISPLAY_TYPE_PCIVGA, 0));
}

int
vga_pci_ioctl(v, cmd, addr, flag, p)
	void *v;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	return (ENOTTY);
}
