/*	$NetBSD: pal.s,v 1.2 1995/11/23 02:34:23 cgd Exp $	*/

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
 * pal_bpt: Breakpoint trap [UNPRIVILEGED]
 */
	.text
LEAF(pal_bpt,0)
	call_pal PAL_bpt
	RET
	END(pal_bpt)

/*
 * pal_bugchk: Bugcheck trap [UNPRIVILEGED]
 */
	.text
LEAF(pal_bugchk,0)
	call_pal PAL_bugchk
	RET
	END(pal_bugchk)

#ifdef ILLEGAL	/* ILLEGAL FROM KERNEL MODE */
/*
 * pal_callsys: System call [UNPRIVILEGED]
 */
	.text
LEAF(pal_callsys,0)
	call_pal PAL_OSF1_callsys
	RET
	END(pal_callsys)
#endif /* ILLEGAL */

/*
 * pal_gentrap: Generate trap [UNPRIVILEGED]
 */
	.text
LEAF(pal_gentrap,0)
	call_pal PAL_gentrap
	RET
	END(pal_gentrap)

/*
 * pal_imb: I-Stream memory barrier. [UNPRIVILEGED]
 * (Makes instruction stream coherent with data stream.)
 */
	.text
LEAF(pal_imb,0)
	call_pal PAL_imb
	RET
	END(pal_imb)

/*
 * pal_rdunique: Read process unique value. [UNPRIVILEGED]
 *
 * Return:
 *	v0	current process unique value
 */
	.text
LEAF(pal_rdunique,0)
	call_pal PAL_rdunique
	RET
	END(pal_rdunique)

/*
 * pal_wrunique: Write process unique value. [UNPRIVILEGED]
 * Arguments:
 *	a0	new process unique value
 */
	.text
LEAF(pal_wrunique,1)
	call_pal PAL_wrunique
	RET
	END(pal_wrunique)

/*
 * pal_draina: Drain aborts. [PRIVILEGED]
 */
	.text
LEAF(pal_draina,0)
	call_pal PAL_draina
	RET
	END(pal_draina)

/*
 * pal_halt: Halt the processor. [PRIVILEGED]
 */
	.text
LEAF(pal_halt,0)
	call_pal PAL_halt
	br	zero,pal_halt	/* Just in case */
	RET
	END(pal_halt)

/*
 * pal_rdps: Read processor status. [PRIVILEGED]
 *
 * Return:
 *	v0	current processor status
 */
	.text
LEAF(pal_rdps,0)
	call_pal PAL_OSF1_rdps
	RET
	END(pal_rdps)

/*
 * pal_rdusp: Read user stack pointer. [PRIVILEGED]
 *
 * Return:
 *	v0	current user stack pointer
 */
	.text
LEAF(pal_rdusp,0)
	call_pal PAL_OSF1_rdusp
	RET
	END(pal_rdusp)

/*
 * pal_rdval: Read system value. [PRIVILEGED]
 *
 * Return:
 *	v0	current system value
 */
	.text
LEAF(pal_rdval,0)
	call_pal PAL_OSF1_rdval
	RET
	END(pal_rdval)

/*
 * pal_retsys: Return from system call. [PRIVILEGED]
 */
	.text
LEAF(pal_retsys,0)
	call_pal PAL_OSF1_retsys
	RET
	END(pal_retsys)

/*
 * pal_rti: Return from trap, fault, or interrupt. [PRIVILEGED]
 */
	.text
LEAF(pal_rti,0)
	call_pal PAL_OSF1_rti
	RET
	END(pal_rti)

/*
 * pal_swpctx: Swap process context. [PRIVILEGED] 
 *
 * Arguments:
 *	a0	new PCB
 *
 * Return:
 *	v0	old PCB
 */
	.text
LEAF(pal_swpctx,1)
	call_pal PAL_OSF1_swpctx
	RET
	END(pal_swpctx)

/*
 * pal_swpipl: Swap Interrupt priority level. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new IPL
 *
 * Return:
 *	v0	old IPL
 */
	.text
LEAF(pal_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(pal_swpipl)

LEAF_NOPROFILE(profile_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(profile_swpipl)

/*
 * pal_tbi: Translation buffer invalidate. [PRIVILEGED]
 *
 * Arguments:
 *	a0	operation selector
 *	a1	address to operate on (if necessary)
 */
	.text
LEAF(pal_tbi,2)
	call_pal PAL_OSF1_tbi
	RET
	END(pal_tbi)

/*
 * pal_whami: Who am I? [PRIVILEGED]
 *
 * Return:
 *	v0	processor number
 */
	.text
LEAF(pal_whami,0)
	call_pal PAL_OSF1_whami
	RET
	END(pal_whami)

/*
 * pal_wrent: Write system entry address. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new vector
 *	a1	vector selector
 */
	.text
LEAF(pal_wrent,2)
	call_pal PAL_OSF1_wrent
	RET
	END(pal_wrent)

/*
 * pal_wrfen: Write floating-point enable. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new enable value (val & 0x1 -> enable).
 */
	.text
LEAF(pal_wrfen,1)
	call_pal PAL_OSF1_wrfen
	RET
	END(pal_wrfen)

/*
 * pal_wrkgp: Write kernel global pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new kernel global pointer
 */
	.text
LEAF(pal_wrkgp,1)
	call_pal PAL_OSF1_wrkgp
	RET
	END(pal_wrkgp)

/*
 * pal_wrusp: Write user stack pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new user stack pointer
 */
	.text
LEAF(pal_wrusp,1)
	call_pal PAL_OSF1_wrusp
	RET
	END(pal_wrusp)

/*
 * pal_wrval: Write system value. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new system value
 */
	.text
LEAF(pal_wrval,1)
	call_pal PAL_OSF1_wrval
	RET
	END(pal_wrval)

/*
 * pal_wrvptptr: Write virtual page table pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new virtual page table pointer
 */
	.text
LEAF(pal_wrvptptr,1)
	call_pal PAL_OSF1_wrvptptr
	RET
	END(pal_wrvptptr)

/*
 * pal_mtpr_mces: Write MCES processor register. [PRIVILEGED, VMS, XXX!]
 *
 * Arguments:
 *	a0	value to write to MCES
 */
	.text
LEAF(pal_mtpr_mces,1)
	call_pal PAL_VMS_mtpr_mces
	RET
	END(pal_mtpr_mces)
