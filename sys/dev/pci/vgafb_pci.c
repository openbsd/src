/*	$OpenBSD: vgafb_pci.c,v 1.2 1998/09/27 05:29:59 rahnds Exp $	*/
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

#include <dev/ic/vgafbvar.h>
#include <dev/pci/vgafb_pcivar.h>

struct vgafb_pci_softc {
	struct device sc_dev; 
 
	pcitag_t sc_pcitag;		/* PCI tag, in case we need it. */
	struct vgafb_config *sc_vc;	/* VGA configuration */ 
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	vgafb_pci_match __P((struct device *, void *, void *));
#else
int	vgafb_pci_match __P((struct device *, struct cfdata *, void *));
#endif
void	vgafb_pci_attach __P((struct device *, struct device *, void *));

int	vgafbpcimmap __P((void *, off_t, int));
int	vgafbpciioctl __P((void *, u_long, caddr_t, int, struct proc *));

struct cfattach vgafb_pci_ca = {
	sizeof(struct vgafb_pci_softc), (cfmatch_t)vgafb_pci_match, vgafb_pci_attach,
};

pcitag_t vgafb_pci_console_tag;
struct vgafb_config vgafb_pci_console_vc;

int
vgafb_pci_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;
	u_int32_t memaddr, memsize;
	u_int32_t ioaddr, iosize;
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
	if (!bcmp(&pa->pa_tag, &vgafb_pci_console_tag, sizeof(pa->pa_tag)))
		return (1);

	/*
	 * If we might match, make sure that the card actually looks OK.
	 */
	memaddr=0xb8000; /* default to isa addresses? */
	ioaddr = 0; 	 /* default to isa addresses? */
	/* needs to do something like the mem_find
	 * below in the ifdef powerpc code. 
	 * should really be done in a machine independant way
	 */
#ifdef powerpc
	{
		int retval;
		u_int32_t cacheable;
		pci_chipset_tag_t pc = pa->pa_pc;

		retval = pci_mem_find(pc, pa->pa_tag, 0x10,
			&memaddr, &memsize, &cacheable);
		if (retval) {
			printf(": couldn't find memory region\n");
			return 0;
		}
#if 0
		printf("vga pci_mem_find returned retval %x A %x S %x C%x\n",
			retval, memaddr, memsize, cacheable);
#endif

{
	int s;
	u_int32_t sizedata;
	/*
	 * Open Firmware (yuck) shuts down devices before entering a
	 * program so we need to bring them back 'online' to respond
         * to bus accesses... so far this is true on the power.4e.
         */
	s = splhigh();
	sizedata = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	sizedata |= (PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE |
		     PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, sizedata);
	splx(s);
}
		ioaddr = 0;
	}
#endif
	if (!vgafb_common_probe(pa->pa_iot, pa->pa_memt,
		ioaddr, memaddr, memsize))
	{
		printf("vgafb_pci_match: common_probe failed\n");
		return (0);
	}

	return (1);
}

void
vgafb_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct vgafb_pci_softc *sc = (struct vgafb_pci_softc *)self;
	struct vgafb_config *vc;
	u_int32_t memaddr, memsize;
	u_int32_t ioaddr, iosize;
	int console;

	memaddr=0xb8000; /* default to isa addresses? */
	ioaddr = 0; 	 /* default to isa addresses? */
#ifdef powerpc
	{
		int retval;
		u_int32_t cacheable;
		pci_chipset_tag_t pc = pa->pa_pc;

		retval = pci_mem_find(pc, pa->pa_tag, 0x10,
			&memaddr, &memsize, &cacheable);
		if (retval) {
			printf(": couldn't find memory region\n");
			return;
		}
	}
	/* powerpc specific hack */
{
	int s;
	u_int32_t sizedata;
	pci_chipset_tag_t pc = pa->pa_pc;
	/*
	 * Open Firmware (yuck) shuts down devices before entering a
	 * program so we need to bring them back 'online' to respond
         * to bus accesses... so far this is true on the power.4e.
         */
	s = splhigh();
	sizedata = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	sizedata |= (PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE |
		     PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, sizedata);
	splx(s);
}
	ioaddr = 0;
#endif
	console = (!bcmp(&pa->pa_tag, &vgafb_pci_console_tag, sizeof(pa->pa_tag)));
	if (console)
		vc = sc->sc_vc = &vgafb_pci_console_vc;
	else {
		vc = sc->sc_vc = (struct vgafb_config *)
		    malloc(sizeof(struct vgafb_config), M_DEVBUF, M_WAITOK);

		/* set up bus-independent VGA configuration */
		vgafb_common_setup(pa->pa_iot, pa->pa_memt, vc, 
		ioaddr, memaddr, memsize);
	}
	vc->vc_mmap = vgafbpcimmap;
	vc->vc_ioctl = vgafbpciioctl;

	sc->sc_pcitag = pa->pa_tag;

	printf("\n");

	vgafb_wscons_attach(self, vc, console);
}

void
vgafb_pci_console(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct vgafb_config *vc = &vgafb_pci_console_vc;
	u_int32_t memaddr, memsize;
	u_int32_t ioaddr, iosize;

	/* for later recognition */
	vgafb_pci_console_tag = pci_make_tag(pc, bus, device, function);

/* XXX probe pci before pci bus config? */
#if 0
	int retval;
	u_int32_t cacheable;
	pci_chipset_tag_t pc = pa->pa_pc;

	retval = pci_mem_find(pc, pa->pa_tag, 0x10,
		&memaddr, &memsize, &cacheable);
	if (retval) {
		printf(": couldn't find memory region\n");
		return 0;
	}
	printf("vga pci_mem_find returned retval %x A %x S %x C%x\n",
		retval, memaddr, memsize, cacheable);

{
	int s;
	u_int32_t sizedata;
	/*
	 * Open Firmware (yuck) shuts down devices before entering a
	 * program so we need to bring them back 'online' to respond
         * to bus accesses... so far this is true on the power.4e.
         */
	s = splhigh();
	sizedata = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	sizedata |= (PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE |
		     PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, sizedata);
	splx(s);
}
	ioaddr = 0;
#endif

	/* set up bus-independent VGA configuration */
	vgafb_common_setup(iot, memt, vc, ioaddr, memaddr, memsize);

	vgafb_wscons_console(vc);
}

int
vgafbpciioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vgafb_pci_softc *sc = v;

	return (vgaioctl(sc->sc_vc, cmd, data, flag, p));
}

int
vgafbpcimmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vgafb_pci_softc *sc = v;

	return (vgammap(sc->sc_vc, offset, prot));
}
