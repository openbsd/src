/*	$OpenBSD: pcb.h,v 1.12 2004/01/12 07:46:16 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * Motorola 88100 pcb definitions
 *
 */
/*
 */
#ifndef _M88K_PCB_H_
#define _M88K_PCB_H_

#include <machine/reg.h>

/*
 * Our PCB is the regular PCB+Save area for kernel frame.
 * Upon entering kernel mode from user land, save the user context
 * in the saved_state area - this is passed as the exception frame.
 * On a context switch, only registers that need to be saved by the
 * C calling convention and few other regs (pc, psr etc) are saved
 * in the kernel_state part of the PCB. Typically, trap frames are
 * saved on the stack (by low level handlers or by hardware) but,
 * we just decided to do it in the PCB.
 */

struct m88100_pcb {
	unsigned pcb_pc;	/* address to return */
	unsigned pcb_ipl;
	unsigned pcb_r14;
	unsigned pcb_r15;
	unsigned pcb_r16;
	unsigned pcb_r17;
	unsigned pcb_r18;
	unsigned pcb_r19;
	unsigned pcb_r20;
	unsigned pcb_r21;
	unsigned pcb_r22;
	unsigned pcb_r23;
	unsigned pcb_r24;
	unsigned pcb_r25;
	unsigned pcb_r26;
	unsigned pcb_r27;
	unsigned pcb_r28;
	unsigned pcb_r29;
	unsigned pcb_r30;
	unsigned pcb_sp; 	/* kernel stack pointer */
	/* floating-point state */
	unsigned pcb_fcr62;
	unsigned pcb_fcr63;
};

struct trapframe {
	union {
		struct reg	r;
	} F_r;
};

#define	tf_r		F_r.r.r
#define	tf_sp		F_r.r.r[31]
#define	tf_epsr		F_r.r.epsr
#define	tf_fpsr		F_r.r.fpsr
#define	tf_fpcr		F_r.r.fpcr
#define	tf_sxip		F_r.r.sxip
#define	tf_snip		F_r.r.snip
#define	tf_sfip		F_r.r.sfip
#define	tf_exip		F_r.r.sxip
#define	tf_enip		F_r.r.snip
#define	tf_ssbr		F_r.r.ssbr
#define	tf_dmt0		F_r.r.dmt0
#define	tf_dmd0		F_r.r.dmd0
#define	tf_dma0		F_r.r.dma0
#define	tf_dmt1		F_r.r.dmt1
#define	tf_dmd1		F_r.r.dmd1
#define	tf_dma1		F_r.r.dma1
#define	tf_dmt2		F_r.r.dmt2
#define	tf_dmd2		F_r.r.dmd2
#define	tf_dma2		F_r.r.dma2
#define	tf_duap		F_r.r.ssbr
#define	tf_dsr		F_r.r.dmt0
#define	tf_dlar		F_r.r.dmd0
#define	tf_dpar		F_r.r.dma0
#define	tf_isr		F_r.r.dmt1
#define	tf_ilar		F_r.r.dmd1
#define	tf_ipar		F_r.r.dma1
#define	tf_isap		F_r.r.dmt2
#define	tf_dsap		F_r.r.dmd2
#define	tf_iuap		F_r.r.dma2
#define	tf_fpecr	F_r.r.fpecr
#define	tf_fphs1	F_r.r.fphs1
#define	tf_fpls1	F_r.r.fpls1
#define	tf_fphs2	F_r.r.fphs2
#define	tf_fpls2	F_r.r.fpls2
#define	tf_fppt		F_r.r.fppt
#define	tf_fprh		F_r.r.fprh
#define	tf_fprl		F_r.r.fprl
#define	tf_fpit		F_r.r.fpit
#define	tf_scratch1	F_r.r.scratch1
#define	tf_vector	F_r.r.vector
#define	tf_mask		F_r.r.mask
#define	tf_mode		F_r.r.mode
#define	tf_ipfsr	F_r.r.ipfsr
#define	tf_dpfsr	F_r.r.dpfsr
#define	tf_cpu		F_r.r.cpu

struct pcb
{
	struct m88100_pcb	kernel_state;
	struct trapframe	user_state;
	int			pcb_onfault;
};

/*
 *	Location of saved user registers for the proc.
 */
#define	USER_REGS(p) \
	(((struct reg *)(&((p)->p_addr->u_pcb.user_state))))
/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps.  Note that the trapframe here is a copy of the one
 * from the top of the kernel stack (included here so that the kernel
 * stack itself need not be dumped).
 */
struct md_coredump {
	struct	trapframe md_tf;
};

#endif /* _M88K_PCB_H_ */
