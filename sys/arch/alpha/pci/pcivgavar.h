/*	$NetBSD: pcivgavar.h,v 1.3 1995/11/23 02:38:13 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#include <dev/pseudo/ansicons.h>

struct pcivga_devconfig {
	__const struct pci_conf_fns *dc_pcf;
	void		*dc_pcfa;
	__const struct pci_mem_fns *dc_pmf;
	void		*dc_pmfa;
	__const struct pci_pio_fns *dc_ppf;
	void		*dc_ppfa;

	pci_tag_t	dc_pcitag;	/* PCI tag */

	u_int16_t	*dc_crtat;	/* VGA screen memory */
	int		dc_iobase;	/* VGA I/O address */

	int		dc_ncol, dc_nrow; /* screen width & height */
	int		dc_ccol, dc_crow; /* current cursor position */

	char		dc_so;		/* in standout mode? */
	char		dc_at;		/* normal attributes */
	char		dc_so_at;	/* standout attributes */

	struct ansicons	dc_ansicons;	/* ansi console emulator info XXX */
};
	
struct pcivga_softc {
	struct	device sc_dev;

	struct	pcivga_devconfig *sc_dc; /* device configuration */
	void	*sc_intr;		/* interrupt handler info */
};

#define	PCIVGA_CURSOR_OFF	-1	/* pass to pcivga_cpos to disable */

#define	DEVICE_IS_PCIVGA(class, id)					\
	    ((PCI_CLASS(class) == PCI_CLASS_DISPLAY &&			\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_DISPLAY_VGA) ||	\
	     (PCI_CLASS(class) == PCI_CLASS_PREHISTORIC &&		\
	      PCI_SUBCLASS(class) == PCI_SUBCLASS_PREHISTORIC_VGA))

void    pcivga_console __P((__const struct pci_conf_fns *, void *, 
	    __const struct pci_mem_fns *, void *,
	    __const struct pci_pio_fns *, void *,
	    pci_bus_t, pci_device_t, pci_function_t));
