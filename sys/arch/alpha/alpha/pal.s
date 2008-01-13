/* $OpenBSD: pal.s,v 1.7 2008/01/13 20:59:52 kettenis Exp $ */
/* $NetBSD: pal.s,v 1.14 1999/12/02 22:08:04 thorpej Exp $ */

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

.file	3 __FILE__
.loc	3 __LINE__

/*
 * The various OSF PALcode routines.
 *
 * The following code is originally derived from pages: (I) 6-5 - (I) 6-7
 * and (III) 2-1 - (III) 2-25 of "Alpha Architecture Reference Manual" by
 * Richard L. Sites.
 *
 * Updates taken from pages: (II-B) 2-1 - (II-B) 2-33 of "Alpha AXP
 * Architecture Reference Manual, Second Edition" by Richard L. Sites
 * and Richard T. Witek.
 */

/*
 * alpha_amask: read architecture features (XXX INSTRUCTION, NOT PALcode OP)
 *
 * Arguments:
 *	a0	bitmask of features to test
 *
 * Returns:
 *	v0	bitmask - bit is _cleared_ if feature is supported
 */
	.text
LEAF(alpha_amask,1)
	amask	a0, v0
	RET
	END(alpha_amask)

/*
 * alpha_implver: read implementation version (XXX INSTRUCTION, NOT PALcode OP)
 *
 * Returns:
 *	v0	implementation version - see <machine/alpha_cpu.h>
 */
	.text
LEAF(alpha_implver,0)
#if 0
	implver	0x1, v0
#else
	.long	0x47e03d80	/* XXX gas(1) does the Wrong Thing */
#endif
	RET
	END(alpha_implver)

/*
 * alpha_pal_cflush: Cache flush [PRIVILEGED]
 *
 * Flush the entire physical page specified by the PFN specified in
 * a0 from any data caches associated with the current processor.
 *
 * Arguments:
 *	a0	page frame number of page to flush
 */
	.text
LEAF(alpha_pal_cflush,1)
	call_pal PAL_cflush
	RET
	END(alpha_pal_cflush)

/*
 * alpha_pal_halt: Halt the processor. [PRIVILEGED]
 */
	.text
LEAF(alpha_pal_halt,0)
	call_pal PAL_halt
	br	zero,alpha_pal_halt	/* Just in case */
	RET
	END(alpha_pal_halt)

/*
 * alpha_pal_rdps: Read processor status. [PRIVILEGED]
 *
 * Return:
 *	v0	current PS value
 */
	.text
LEAF(alpha_pal_rdps,0)
	call_pal PAL_OSF1_rdps
	RET
	END(alpha_pal_rdps)

/*
 * alpha_pal_swpipl: Swap Interrupt priority level. [PRIVILEGED]
 * _alpha_pal_swpipl: Same, from profiling code. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new IPL
 *
 * Return:
 *	v0	old IPL
 */
	.text
LEAF(alpha_pal_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(alpha_pal_swpipl)

LEAF_NOPROFILE(_alpha_pal_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(_alpha_pal_swpipl)

/*
 * alpha_pal_wrent: Write system entry address. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new vector
 *	a1	vector selector
 */
	.text
LEAF(alpha_pal_wrent,2)
	call_pal PAL_OSF1_wrent
	RET
	END(alpha_pal_wrent)

/*
 * alpha_pal_wrvptptr: Write virtual page table pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new virtual page table pointer
 */
	.text
LEAF(alpha_pal_wrvptptr,1)
	call_pal PAL_OSF1_wrvptptr
	RET
	END(alpha_pal_wrvptptr)
