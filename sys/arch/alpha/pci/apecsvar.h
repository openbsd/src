/*	$NetBSD: apecsvar.h,v 1.1 1995/11/23 02:37:21 cgd Exp $	*/

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
 * An APECS chipset's configuration.
 *
 * All of the information that the chipset-specific functions need to
 * do their dirty work (and more!).
 */
struct apecs_config {
	int	ac_comanche_pass2;
	int	ac_epic_pass2;
	int	ac_memwidth;

	__const struct pci_conf_fns	*ac_conffns;
	void				*ac_confarg;

	__const struct pci_dma_fns	*ac_dmafns;
	void				*ac_dmaarg;

	__const struct pci_intr_fns	*ac_intrfns;
	void				*ac_intrarg;

	__const struct pci_mem_fns	*ac_memfns;
	void				*ac_memarg;

	__const struct pci_pio_fns	*ac_piofns;
	void				*ac_pioarg;
};

struct apecs_softc {
	struct	device sc_dev;

	struct	apecs_config *sc_acp;
};

extern __const struct pci_conf_fns	apecs_conf_fns;
extern __const struct pci_dma_fns	apecs_dma_fns;
/* pci interrupt functions handled elsewhere */
extern __const struct pci_mem_fns	apecs_mem_fns;
extern __const struct pci_pio_fns	apecs_pio_fns;

void	apecs_init __P((struct apecs_config *));
