/*	$OpenBSD: pcivgavar.h,v 1.6 1996/12/08 00:20:44 niklas Exp $	*/
/*	$NetBSD: pcivgavar.h,v 1.6 1996/10/23 04:12:30 cgd Exp $	*/

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

struct pcivga_devconfig {
	bus_space_tag_t	dc_iot;
	bus_space_tag_t	dc_memt;
	pci_chipset_tag_t dc_pc;

	pcitag_t	dc_pcitag;	/* PCI tag */

	bus_space_handle_t dc_ioh, dc_memh;

	int		dc_ncol, dc_nrow; /* screen width & height */
	int		dc_ccol, dc_crow; /* current cursor position */

	char		dc_so;		/* in standout mode? */
	char		dc_at;		/* normal attributes */
	char		dc_so_at;	/* standout attributes */
};
	
struct pcivga_softc {
	struct	device sc_dev;

	struct	pcivga_devconfig *sc_dc; /* device configuration */
	void	*sc_intr;		/* interrupt handler info */
};

#define	DEVICE_IS_PCIVGA(class, id)					\
	    (((PCI_CLASS(class) == PCI_CLASS_DISPLAY &&			\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_DISPLAY_VGA) ||	\
	     (PCI_CLASS(class) == PCI_CLASS_PREHISTORIC &&		\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_PREHISTORIC_VGA)) ? 1 : 0)

void    pcivga_console __P((bus_space_tag_t, bus_space_tag_t,
	    pci_chipset_tag_t, int, int, int));
