/*	$OpenBSD: pal.s,v 1.4 1996/10/30 22:38:18 niklas Exp $	*/
/*	$NetBSD: pal.s,v 1.5 1996/07/14 04:21:53 cgd Exp $	*/

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
 * The various OSF PALcode routines.
 *
 * The following code is derived from pages: (I) 6-5 - (I) 6-7 and
 * (III) 2-1 - (III) 2-25 of "Alpha Architecture Reference Manual" by
 * Richard L. Sites.
 */

/*
 * alpha_rpcc: read process cycle counter (XXX INSTRUCTION, NOT PALcode OP)
 */
        .text
LEAF(alpha_rpcc,1)
        rpcc    v0
        RET
        END(alpha_rpcc)

/*
 * alpha_mb: memory barrier (XXX INSTRUCTION, NOT PALcode OP)
 */
	.text
LEAF(alpha_mb,0)
	mb
	RET
	END(alpha_mb)

/*
 * alpha_wmb: write memory barrier (XXX INSTRUCTION, NOT PALcode OP)
 */
	.text
LEAF(alpha_wmb,0)
	/* wmb XXX */
	mb /* XXX */
	RET
	END(alpha_wmb)

/*
 * alpha_pal_imb: I-Stream memory barrier. [UNPRIVILEGED]
 * (Makes instruction stream coherent with data stream.)
 */
	.text
LEAF(alpha_pal_imb,0)
	call_pal PAL_imb
	RET
	END(alpha_pal_imb)

/*
 * alpha_pal_draina: Drain aborts. [PRIVILEGED]
 */
	.text
LEAF(alpha_pal_draina,0)
	call_pal PAL_draina
	RET
	END(alpha_pal_draina)

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
 * alpha_pal_rdmces: Read MCES processor register. [PRIVILEGED]
 *
 * Return:
 *	v0	current MCES value
 */
	.text
LEAF(alpha_pal_rdmces,1)
	call_pal PAL_OSF1_rdmces
	RET
	END(alpha_pal_rdmces)

/*
 * alpha_pal_rdusp: Read user stack pointer. [PRIVILEGED]
 *
 * Return:
 *	v0	current user stack pointer
 */
	.text
LEAF(alpha_pal_rdusp,0)
	call_pal PAL_OSF1_rdusp
	RET
	END(alpha_pal_rdusp)

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
 * alpha_pal_tbi: Translation buffer invalidate. [PRIVILEGED]
 *
 * Arguments:
 *	a0	operation selector
 *	a1	address to operate on (if necessary)
 */
	.text
LEAF(alpha_pal_tbi,2)
	call_pal PAL_OSF1_tbi
	RET
	END(alpha_pal_tbi)

/*
 * alpha_pal_whami: Who am I? [PRIVILEGED]
 *
 * Return:
 *	v0	processor number
 */
	.text
LEAF(alpha_pal_whami,0)
	call_pal PAL_OSF1_whami
	RET
	END(alpha_pal_whami)

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
 * alpha_pal_wrfen: Write floating-point enable. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new enable value (val & 0x1 -> enable).
 */
	.text
LEAF(alpha_pal_wrfen,1)
	call_pal PAL_OSF1_wrfen
	RET
	END(alpha_pal_wrfen)

/*
 * alpha_pal_wrusp: Write user stack pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new user stack pointer
 */
	.text
LEAF(alpha_pal_wrusp,1)
	call_pal PAL_OSF1_wrusp
	RET
	END(alpha_pal_wrusp)

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

/*
 * alpha_pal_wrmces: Write MCES processor register. [PRIVILEGED]
 *
 * Arguments:
 *	a0	value to write to MCES
 */
	.text
LEAF(alpha_pal_wrmces,1)
	call_pal PAL_OSF1_wrmces
	RET
	END(alpha_pal_wrmces)
