/*	$NetBSD: ioasicvar.h,v 1.2 1996/03/17 21:37:45 jonathan Exp $	*/

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
 * IOASIC subdevice attachment information.
 */

/* Attachment arguments. */
struct ioasicdev_attach_args {
	char	iada_modname[TC_ROM_LLEN];
	tc_offset_t iada_offset;
	tc_addr_t iada_addr;
	void	*iada_cookie;
};

/* Device locators. */
#define	ioasiccf_offset	cf_loc[0]		/* offset */

#define	IOASIC_OFFSET_UNKNOWN	-1

/*
 * The IOASIC (bus) cfdriver, so that subdevices can more
 * easily tell what bus they're on.
 */
extern struct cfdriver ioasic_cd;


/*
 * XXX Some drivers need direct access to IOASIC registers.
 */
extern tc_addr_t ioasic_base;


/*
 * Interrupt establishment/disestablishment functions
 */
void    ioasic_intr_establish __P((struct device *, void *, tc_intrlevel_t,
	    int (*)(void *), void *));
void    ioasic_intr_disestablish __P((struct device *, void *));


/*
 * Miscellaneous helper functions.
 */
int	ioasic_submatch __P((struct cfdata *, struct ioasicdev_attach_args *));
char	*ioasic_lance_ether_address __P((void));
void	ioasic_lance_dma_setup __P((void *));
