/*	$NetBSD: isa_intr.h,v 1.1 1995/08/02 14:44:57 niklas Exp $	*/

/*
 * Copyright (c) 1995 Niklas Hallqvist.
 * All rights reserved.
 *
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Chris G. Demetriou, Niklas Hallqvist.
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

/* Prototypes for ISA-ish I/O interrupt functions. */

/*
 * XXX
 * XXX THIS WILL LIKELY HAVE TO BE COMPLETELY CHANGED.
 * XXX
 */

struct	isa_intr_fcns {
	void	(*isa_intr_setup) __P((void));

	void	*(*isa_intr_establish) __P((int irq, int type, int level,
	    int (*ih_fun)(void *), void *ih_arg));
	void	(*isa_intr_disestablish) __P((void *handler));

	void	(*isa_iointr) __P((void *framep, int vec));
};

/*
 * Global which tells which set of functions are correct
 * for this machine.
 */
struct	isa_intr_fcns *isa_intr_fcns;

extern struct isa_intr_fcns ggbus_intr_fcns;
extern struct isa_intr_fcns cross_intr_fcns;
