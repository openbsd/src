/*	$NetBSD: ciavar.h,v 1.1 1995/11/23 02:37:35 cgd Exp $	*/

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

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>

/*
 * A 21171 chipset's configuration.
 *
 * All of the information that the chipset-specific functions need to
 * do their dirty work (and more!).
 */
struct cia_config {
	__const struct pci_conf_fns	*cc_conffns;
	void				*cc_confarg;

	__const struct pci_dma_fns	*cc_dmafns;
	void				*cc_dmaarg;

	__const struct pci_intr_fns	*cc_intrfns;
	void				*cc_intrarg;

	__const struct pci_mem_fns	*cc_memfns;
	void				*cc_memarg;

	__const struct pci_pio_fns	*cc_piofns;
	void				*cc_pioarg;
};

struct cia_softc {
	struct	device sc_dev;

	struct	cia_config *sc_ccp;
	/* XXX SGMAP info */
};

extern __const struct pci_conf_fns	cia_conf_fns;
extern __const struct pci_dma_fns	cia_dma_fns;
/* pci interrupt functions handled elsewhere */
extern __const struct pci_mem_fns	cia_mem_fns;
extern __const struct pci_pio_fns	cia_pio_fns;

void	cia_init __P((struct cia_config *));
