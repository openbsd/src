/*	$NetBSD: pci_chipset.h,v 1.3 1995/08/03 01:17:14 cgd Exp $	*/

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

/*
 * Function switch to implement the various PCI bus interfaces.
 * XXX this probably needs some work...
 */

struct pci_cs_fcns {
	void		(*cs_setup) __P((void));
	pcitag_t	(*cs_make_tag) __P((int, int, int));
	pcireg_t	(*cs_conf_read) __P((pcitag_t, int));
	void		(*cs_conf_write) __P((pcitag_t, int, pcireg_t));
	int		(*cs_map_io) __P((pcitag_t, int, int *));
	int		(*cs_map_mem) __P((pcitag_t, int, vm_offset_t *,
			    vm_offset_t *));
	int		(*cs_pcidma_map) __P((caddr_t, vm_size_t,
			    vm_offset_t *));
	void		(*cs_pcidma_unmap) __P((caddr_t, vm_size_t, int,
			    vm_offset_t *));
};

struct pci_cs_fcns *pci_cs_fcns;
extern struct pci_cs_fcns apecs_p1e_cs_fcns;
extern struct pci_cs_fcns apecs_p2e_cs_fcns;
extern struct pci_cs_fcns lca_cs_fcns;


/*
 * Function switch to implement the various PCI configuration schemes.
 * XXX this probably needs some work...
 */

struct pci_cfg_fcns {
	void		(*cfg_attach) __P((struct device *, struct
			    device *, void *));
	void		*(*cfg_map_int) __P((pcitag_t, pci_intrlevel,
			    int (*) (void *), void *, int));
};

struct pci_cfg_fcns *pci_cfg_fcns;
extern struct pci_cfg_fcns pci_2100_a50_sio1_cfg_fcns;
extern struct pci_cfg_fcns pci_2100_a50_sio2_cfg_fcns;

/*
 * Miscellaneous functions.
 */
isa_intrlevel	pcilevel_to_isa __P((pci_intrlevel));
