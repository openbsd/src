/*	$NetBSD: if_levar.h,v 1.3 1996/10/21 22:56:46 thorpej Exp $	*/

/*
 * LANCE Ethernet driver header file
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, Paul Richards. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */

#define	PCNET_PCI_RDP	0x10
#define	PCNET_PCI_RAP	0x12

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom.ac_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	void	*sc_ih;
	bus_space_tag_t sc_iot;		/* space cookie */
	bus_space_handle_t sc_ioh;	/* bus space handle */
	int	sc_rap, sc_rdp;		/* offsets to LANCE registers */
};
