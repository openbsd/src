/*	$OpenBSD: netbsd_machdep.h,v 1.2 1999/09/14 01:05:24 kstailey Exp $	*/

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

#ifndef _NETBSD_MACHDEP_H
#define _NETBSD_MACHDEP_H

/*
 * signal support
 */

struct netbsd_sigcontext {
	long	sc_onstack;		/* sigstack state to restore */
	long	__sc_mask13;		/* signal mask to restore (old style) */
	long	sc_pc;			/* pc to restore */
	long	sc_ps;			/* ps to restore */
	unsigned long sc_regs[32];	/* integer register set (see above) */
#define	sc_sp	sc_regs[R_SP]
	long	sc_ownedfp;		/* fp has been used */
	unsigned long sc_fpregs[32];	/* FP register set (see above) */
	unsigned long sc_fpcr;		/* FP control register (see above) */
	unsigned long sc_fp_control;	/* FP software control word */
	long	sc_reserved[2];		/* XXX */
	long	sc_xxx[8];		/* XXX */
	netbsd_sigset_t sc_mask;	/* signal mask to restore (new style) */
};

#endif /* _NETBSD_MACHDEP_H */
