/*	$NetBSD: pcb.h,v 1.1 1995/02/13 23:07:43 cgd Exp $	*/

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

#include <machine/frame.h>
#include <machine/reg.h>

/*
 * XXX where did this info come from?
 */

/*
 * PCB: process control block
 *
 * In this case, the hardware structure that is the defining element
 * for a process, and the additional state that must be saved by software
 * on a context switch.  Fields marked [HW] are mandated by hardware; fields
 * marked [SW] are for the software.
 *
 * It's said in the VMS PALcode section of the AARM that the pcb address
 * passed to the swpctx PALcode call has to be a physical address.  Not
 * knowing this (and trying a virtual) address proved this correct.
 * So we cache the physical address of the pcb in the md_proc struct.
 */
struct pcb {
	u_int64_t	pcb_ksp;		/* kernel stack ptr	[HW] */
	u_int64_t	pcb_usp;		/* user stack ptr	[HW] */
	u_int64_t	pcb_ptbr;		/* page table base reg	[HW] */
	u_int32_t	pcb_pcc;		/* process cycle cntr	[HW] */
	u_int32_t	pcb_asn;		/* address space number	[HW] */
	u_int64_t	pcb_unique;		/* process unique value	[HW] */
	u_int64_t	pcb_fen;		/* FP enable (in bit 0)	[HW] */
	u_int64_t	pcb_decrsv[2];		/* DEC reserved		[HW] */
	u_int64_t	pcb_context[9];		/* s[0-6], ra, ps	[SW] */
	struct fpreg	pcb_fp;			/* FP registers		[SW] */
	caddr_t		pcb_onfault;		/* for copy faults	[SW] */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the Alpha, that's a trap frame and the floating
 * point registers.
 */
struct md_coredump {
	struct	trapframe md_tf;
};
