/*	$OpenBSD: vga_pci.c,v 1.6 1997/11/06 12:26:56 niklas Exp $	*/
/*	$NetBSD: vga_pci.c,v 1.4 1996/12/05 01:39:38 cgd Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#ifndef i386
#include <machine/autoconf.h>
#endif
#include <machine/pte.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

struct vga_pci_softc {
	struct device sc_dev; 
 
	pcitag_t sc_pcitag;		/* PCI tag, in case we need it. */
	struct vga_config *sc_vc;	/* VGA configuration */ 
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	vga_pci_match __P((struct device *, void *, void *));
#else
int	vga_pci_match __P((struct device *, struct cfdata *, void *));
#endif
void	vga_pci_attach __P((struct device *, struct device *, void *));

int	vgapcimmap __P((void *, off_t, int));
int	vgapciioctl __P((void *, u_long, caddr_t, int, struct proc *));

struct cfattach vga_pci_ca = {
	sizeof(struct vga_pci_softc), (cfmatch_t)vga_pci_match, vga_pci_attach,
};

pcitag_t vga_pci_console_tag;
struct vga_config vga_pci_console_vc;

int
vga_pci_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
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

	/* If it's the console, we have a winner! */
	if (!bcmp(&pa->pa_tag, &vga_pci_console_tag, sizeof(pa->pa_tag)))
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
	struct vga_config *vc;
	char devinfo[256];
	int console;

	console = (!bcmp(&pa->pa_tag, &vga_pci_console_tag, sizeof(pa->pa_tag)));
	if (console)
		vc = sc->sc_vc = &vga_pci_console_vc;
	else {
		vc = sc->sc_vc = (struct vga_config *)
		    malloc(sizeof(struct vga_config), M_DEVBUF, M_WAITOK);

		/* set up bus-independent VGA configuration */
		vga_common_setup(pa->pa_iot, pa->pa_memt, vc);
	}
	vc->vc_mmap = vgapcimmap;
	vc->vc_ioctl = vgapciioctl;

	sc->sc_pcitag = pa->pa_tag;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pa->pa_class));

	vga_wscons_attach(self, vc, console);
}

void
vga_pci_console(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct vga_config *vc = &vga_pci_console_vc;

	/* for later recognition */
	vga_pci_console_tag = pci_make_tag(pc, bus, device, function);

	/* set up bus-independent VGA configuration */
	vga_common_setup(iot, memt, vc);

	vga_wscons_console(vc);
}

int
vgapciioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vga_pci_softc *sc = v;

	return (vgaioctl(sc->sc_vc, cmd, data, flag, p));
}

int
vgapcimmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vga_pci_softc *sc = v;

	return (vgammap(sc->sc_vc, offset, prot));
}
