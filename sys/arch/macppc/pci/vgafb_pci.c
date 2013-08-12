/*	$OpenBSD: vgafb_pci.c,v 1.33 2013/08/12 08:38:03 mpi Exp $	*/
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
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <arch/macppc/pci/vgafbvar.h>
#include <dev/pci/vga_pcivar.h>

#ifdef DEBUG_VGAFB
#define DPRINTF(x...)	do { printf(x); } while (0);
#else
#define DPRINTF(x...)
#endif

int	vgafb_pci_match(struct device *, void *, void *);
void	vgafb_pci_attach(struct device *, struct device *, void *);

void	vgafb_pci_mem_init(struct vga_pci_softc *, uint32_t *, uint32_t *,
	    uint32_t *, uint32_t *);

const struct cfattach vgafb_pci_ca = {
	sizeof(struct vga_pci_softc), vgafb_pci_match, vgafb_pci_attach,
};

struct vga_config vgafbcn;

void
vgafb_pci_mem_init(struct vga_pci_softc *dev, uint32_t *memaddr,
    uint32_t *memsize, uint32_t *mmioaddr, uint32_t *mmiosize)
{
	struct vga_pci_bar *bar;
	int i;

	*memsize  = 0x0;
	*mmiosize = 0x0;

	for (i = 0; i < VGA_PCI_MAX_BARS; i++) {
		bar = dev->bars[i];
		if (bar == NULL)
			continue;

		DPRINTF("\nvgafb: 0x%04x: BAR ", bar->addr);

		switch (PCI_MAPREG_TYPE(bar->maptype)) {
		case PCI_MAPREG_TYPE_IO:
			DPRINTF("io ");
			break;
		case PCI_MAPREG_TYPE_MEM:
			if (bar->base == 0 || bar->maxsize == 0) {
				/* ignore this entry */
			} else if (*memsize == 0) {
				/*
				 * first memory slot found goes into memory,
				 * this is for the case of no mmio
				 */
				*memaddr = bar->base;
				*memsize = bar->maxsize;
			} else {
				/*
				 * Oh, we have a second 'memory'
				 * region, is this region the vga memory
				 * or mmio, we guess that memory is
				 * the larger of the two.
				 */
				 if (*memsize >= bar->maxsize) {
					/* this is the mmio */
					*mmioaddr = bar->base;
					*mmiosize = bar->maxsize;
				 } else {
					/* this is the memory */
					*mmioaddr = *memaddr;
					*mmiosize = *memsize;
					*memaddr = bar->base;
					*memsize = bar->maxsize;
				 }
			}
			DPRINTF("mem ");
			break;
		}

		if (bar->maptype == PCI_MAPREG_MEM_TYPE_64BIT) {
			DPRINTF("64bit");
			i++;
		} else {
			DPRINTF("addr: 0x%08x/0x%08x", bar->base, bar->maxsize);
		}
	}

	/* ATI driver maps 0x80000 mmio, grr */
	if (*mmiosize > 0 && *mmiosize < 0x80000) {
		*mmiosize = 0x80000;
	}

	DPRINTF("\nvgafb: memaddr %x, memsize %x, mmioaddr %x, mmiosize %x",
	    *memaddr, *memsize, *mmioaddr, *mmiosize);
}

int
vgafb_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int node;

	if (DEVICE_IS_VGA_PCI(pa->pa_class) == 0) {
		/*
		 * XXX Graphic cards found in iMac G3 have a ``Misc''
		 * subclass, match them all.
		 */
		if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
		    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_MISC)
			return (0);
	}

	/*
	 * XXX Non-console devices do not get configured by the PROM,
	 * XXX so do not attach them yet.
	 */
	node = PCITAG_NODE(pa->pa_tag);
	if (!vgafb_is_console(node))
		return (0);

	return (1);
}

void
vgafb_pci_attach(struct device *parent, struct device  *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;
	struct vga_config *vc;
	u_int32_t memaddr, memsize;
	u_int32_t mmioaddr, mmiosize;
	int console, node;
	pcireg_t reg;


	node = PCITAG_NODE(pa->pa_tag);

 	vga_pci_bar_init(sc, pa);
	vgafb_pci_mem_init(sc, &memaddr, &memsize, &mmioaddr, &mmiosize);


	console = vgafb_is_console(node);
	if (console) {
		vc = sc->sc_vc = &vgafbcn;

		/*
		 * The previous size was not necessarily the real size
		 * but what is needed for the glass console.
		 */
		vc->membase = memaddr;
		vc->memsize = memsize;
	} else {
		vc = sc->sc_vc = (struct vga_config *)
		    malloc(sizeof(struct vga_config), M_DEVBUF, M_WAITOK);

		/* set up bus-independent VGA configuration */
		vgafb_init(pa->pa_iot, pa->pa_memt, vc, memaddr, memsize);
	}

	if (mmiosize != 0) {
		vc->mmiobase = mmioaddr;
		vc->mmiosize = mmiosize;

		printf (", mmio");
	}
	printf("\n");

	/*
	 * Enable bus master; X might need this for accelerated graphics.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	vgafb_wsdisplay_attach(self, vc, console);
}
