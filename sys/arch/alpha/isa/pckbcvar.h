/*	$OpenBSD: pckbcvar.h,v 1.1 1999/01/08 03:16:15 niklas Exp $	*/
/*	$NetBSD: pckbcvar.h,v 1.1 1996/11/25 03:26:37 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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

struct pckbc_attach_args {
	unsigned int	pa_slot;
	/* XXX should have a device type number */
	/* XXX should have a cookie to be passed to callbacks */

	/* XXX THESE DO NOT BELONG */
	bus_space_tag_t	pa_iot;
	bus_space_handle_t pa_ioh;
	bus_space_handle_t pa_pit_ioh;
	bus_space_handle_t pa_delaybah;
	isa_chipset_tag_t pa_ic;
};

#define	PCKBC_KBD_SLOT	0
#define	PCKBC_AUX_SLOT	1
