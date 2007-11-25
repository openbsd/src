/* $OpenBSD: vga_pcivar.h,v 1.9 2007/11/25 17:11:12 oga Exp $ */
/* $NetBSD: vga_pcivar.h,v 1.1 1998/03/22 15:16:19 drochner Exp $ */

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

#ifndef _PCI_VGA_PCIVAR_H_
#define _PCI_VGA_PCIVAR_H_

#define	DEVICE_IS_VGA_PCI(class)					\
	    (((PCI_CLASS(class) == PCI_CLASS_DISPLAY &&			\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_DISPLAY_VGA) ||	\
	     (PCI_CLASS(class) == PCI_CLASS_PREHISTORIC &&		\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_PREHISTORIC_VGA)) ? 1 : 0)


struct vga_pci_softc {
	struct device sc_dev;

#if 0
	struct vga_config *sc_vc;	/* VGA configuration */
#endif
#ifdef VESAFB
	int sc_width;
	int sc_height;
	int sc_depth;
	int sc_linebytes;
	u_int32_t sc_base;
	int sc_mode;			/* WSDISPLAY_MODE_EMUL or _DUMBFB */
	int sc_textmode;		/* original VESA text mode */
	int sc_gfxmode;			/* VESA graphics mode */
	u_char sc_cmap_red[256];	/* saved color map */
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];

#endif
};

int vga_pci_cnattach(bus_space_tag_t, bus_space_tag_t,
			  pci_chipset_tag_t, int, int, int);

#ifdef VESAFB
int vesafb_find_mode(struct vga_pci_softc *, int, int, int);
void vesafb_set_mode(struct vga_pci_softc *, int);
int vesafb_get_mode(struct vga_pci_softc *);
int vesafb_get_supported_depth(struct vga_pci_softc *);
#endif

#endif /* _PCI_VGA_PCIVAR_H_ */
