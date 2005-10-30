/*	$OpenBSD: pcb.h,v 1.8 2005/10/30 20:55:50 miod Exp $	*/
/*	$NetBSD: pcb.h,v 1.5 1996/11/13 22:21:00 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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

#ifndef _ALPHA_PCB_H_
#define _ALPHA_PCB_H_

#include <machine/frame.h>
#include <machine/reg.h>

#include <machine/alpha_cpu.h>

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
	struct alpha_pcb pcb_hw;		/* PALcode defined */
	unsigned long	pcb_context[9];		/* s[0-6], ra, ps	[SW] */
	struct fpreg	pcb_fp;			/* FP registers		[SW] */
	unsigned long	pcb_onfault;		/* for copy faults	[SW] */
	struct cpu_info *__volatile pcb_fpcpu;	/* CPU with our FP state[SW] */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the Alpha, that's a trap frame and the floating
 * point registers.
 */
struct md_coredump {
	struct	trapframe md_tf;
	struct	fpreg md_fpstate;
};

#ifdef _KERNEL
void savectx(struct pcb *);
#endif

#endif /* _ALPHA_PCB_H_ */
