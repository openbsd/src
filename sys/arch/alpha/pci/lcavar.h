/*	$NetBSD: lcavar.h,v 1.1 1995/11/23 02:37:47 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
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
 * LCA chipset's configuration.
 *
 * All of the information that the chipset-specific functions need to
 * do their dirty work (and more!).
 */
struct lca_config {
	__const struct pci_conf_fns	*lc_conffns;
	void				*lc_confarg;

	__const struct pci_dma_fns	*lc_dmafns;
	void				*lc_dmaarg;

	__const struct pci_intr_fns	*lc_intrfns;
	void				*lc_intrarg;

	__const struct pci_mem_fns	*lc_memfns;
	void				*lc_memarg;

	__const struct pci_pio_fns	*lc_piofns;
	void				*lc_pioarg;
};

struct lca_softc {
	struct	device sc_dev;

	struct	lca_config *sc_lcp;
};

extern __const struct pci_conf_fns	lca_conf_fns;
extern __const struct pci_dma_fns	lca_dma_fns;
/* pci interrupt functions handled elsewhere */
extern __const struct pci_mem_fns	lca_mem_fns;
extern __const struct pci_pio_fns	lca_pio_fns;

void	lca_init __P((struct lca_config *));
