/*	$OpenBSD: lcavar.h,v 1.5 1996/12/08 00:20:38 niklas Exp $	*/
/*	$NetBSD: lcavar.h,v 1.4 1996/10/23 04:12:26 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
	bus_space_tag_t lc_iot, lc_memt;
	struct alpha_pci_chipset lc_pc;
};

struct lca_softc {
	struct	device sc_dev;

	struct	lca_config *sc_lcp;
};

void	lca_init __P((struct lca_config *));
void	lca_pci_init __P((pci_chipset_tag_t, void *));

bus_space_tag_t apecs_lca_bus_io_init __P((void *));
bus_space_tag_t apecs_lca_bus_mem_init __P((void *));
