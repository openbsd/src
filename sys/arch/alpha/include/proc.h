/*	$OpenBSD: proc.h,v 1.7 2002/03/14 01:26:27 millert Exp $	*/
/*	$NetBSD: proc.h,v 1.2 1995/03/24 15:01:36 cgd Exp $	*/

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

#include <machine/cpu.h>
/*
 * Machine-dependent part of the proc struct for the Alpha.
 */

struct mdbpt {
	vaddr_t	addr;
	u_int32_t contents;
};

struct mdproc {
	u_long md_flags;
	struct trapframe *md_tf;	/* trap/syscall registers */
	struct pcb *md_pcbpaddr;	/* phys addr of the pcb */
	struct mdbpt md_sstep[2];	/* two breakpoints for sstep */
};

#define	MDP_FPUSED	0x0001		/* Process used the FPU */
#define MDP_STEP1	0x0002		/* Single step normal */
#define MDP_STEP2	0x0003		/* Single step branch */

#ifdef _KERNEL
void switch_exit(struct proc *);
#endif
