/*	$NetBSD: tc.h,v 1.2 1995/12/28 08:42:16 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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
 * TurboChannel-specific functions and structures.
 */

#define	TC_SLOT_ROM		0x000003e0
#define	TC_SLOT_PROTOROM	0x003c03e0

#include <machine/tc_machdep.h>
#include <dev/tc/tcreg.h>


/*
 * Obsolete (pre-NetBSD/1.1-style) tc bus descriptor
 * and device-attach arguments.
 */

/* The contents of a cfdata->cf_loc for a TurboChannel device */
struct tc_cfloc {
	int	cf_slot;		/* Slot number */
	int	cf_offset;		/* XXX? Offset into slot. */
	int	cf_vec;
	int	cf_ipl;
};

#define	TC_SLOT_WILD	-1		/* wildcarded slot */
#define	TC_OFFSET_WILD	-1		/* wildcarded offset */
#define TC_VEC_WILD	-1		/* wildcarded vec */
#define TC_IPL_WILD	-1		/* wildcarded ipl */

struct tc_slot_desc {
	caddr_t	tsd_dense;
};

struct tc_cpu_desc {
	struct	tc_slot_desc *tcd_slots;
	long	tcd_nslots;
	struct	confargs *tcd_devs;
	long	tcd_ndevs;
	void	(*tc_intr_setup) __P((void));
	void	(*tc_intr_establish)
		    __P((struct confargs *, intr_handler_t, void *));
	void	(*tc_intr_disestablish) __P((struct confargs *));
	int	(*tc_iointr) __P((u_int mask, u_int pc,
				  u_int statusReg, u_int causeReg));
};

int	tc_intrnull __P((void *));
