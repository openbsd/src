/*	$OpenBSD: pcb.h,v 1.1.1.1 2004/04/21 15:23:57 aoyama Exp $ */
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
	struct reg	tf_regs;
	register_t	tf_vector;	/* exception vector number */
	register_t	tf_mask;	/* interrupt mask level */
	register_t	tf_mode;	/* interrupt mode */
	register_t	tf_scratch1;	/* reserved for use by locore */
	register_t	tf_ipfsr;	/* P BUS status */
	register_t	tf_dpfsr;	/* P BUS status */
	register_t	tf_cpu;		/* cpu number */
};

#define	tf_r		tf_regs.r
#define	tf_sp		tf_regs.r[31]
#define	tf_epsr		tf_regs.epsr
#define	tf_fpsr		tf_regs.fpsr
#define	tf_fpcr		tf_regs.fpcr
#define	tf_sxip		tf_regs.sxip
#define	tf_snip		tf_regs.snip
#define	tf_sfip		tf_regs.sfip
#define	tf_exip		tf_regs.sxip
#define	tf_enip		tf_regs.snip
#define	tf_ssbr		tf_regs.ssbr
#define	tf_dmt0		tf_regs.dmt0
#define	tf_dmd0		tf_regs.dmd0
#define	tf_dma0		tf_regs.dma0
#define	tf_dmt1		tf_regs.dmt1
#define	tf_dmd1		tf_regs.dmd1
#define	tf_dma1		tf_regs.dma1
#define	tf_dmt2		tf_regs.dmt2
#define	tf_dmd2		tf_regs.dmd2
#define	tf_dma2		tf_regs.dma2
#define	tf_duap		tf_regs.ssbr
#define	tf_dsr		tf_regs.dmt0
#define	tf_dlar		tf_regs.dmd0
#define	tf_dpar		tf_regs.dma0
#define	tf_isr		tf_regs.dmt1
#define	tf_ilar		tf_regs.dmd1
#define	tf_ipar		tf_regs.dma1
#define	tf_isap		tf_regs.dmt2
#define	tf_dsap		tf_regs.dmd2
#define	tf_iuap		tf_regs.dma2
#define	tf_fpecr	tf_regs.fpecr
#define	tf_fphs1	tf_regs.fphs1
#define	tf_fpls1	tf_regs.fpls1
#define	tf_fphs2	tf_regs.fphs2
#define	tf_fpls2	tf_regs.fpls2
#define	tf_fppt		tf_regs.fppt
#define	tf_fprh		tf_regs.fprh
#define	tf_fprl		tf_regs.fprl
#define	tf_fpit		tf_regs.fpit

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
