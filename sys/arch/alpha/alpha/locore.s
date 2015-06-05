/* $OpenBSD: locore.s,v 1.39 2015/06/05 18:36:07 deraadt Exp $ */
/* $NetBSD: locore.s,v 1.94 2001/04/26 03:10:44 ross Exp $ */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

.file	1 __FILE__

#include <machine/asm.h>

#include "assym.h"

#if defined(MULTIPROCESSOR)

/*
 * Get various per-cpu values.  A pointer to our cpu_info structure
 * is stored in SysValue.  These macros clobber v0, t0, t8..t11.
 *
 * All return values are in v0.
 */
#define	GET_CPUINFO		call_pal PAL_OSF1_rdval

#define	GET_CURPROC							\
	call_pal PAL_OSF1_rdval					;	\
	addq	v0, CPU_INFO_CURPROC, v0

#define	GET_FPCURPROC							\
	call_pal PAL_OSF1_rdval					;	\
	addq	v0, CPU_INFO_FPCURPROC, v0

#define	GET_CURPCB							\
	call_pal PAL_OSF1_rdval					;	\
	addq	v0, CPU_INFO_CURPCB, v0

#define	GET_IDLE_PCB(reg)						\
	call_pal PAL_OSF1_rdval					;	\
	ldq	reg, CPU_INFO_IDLE_PCB_PADDR(v0)

#else	/* if not MULTIPROCESSOR... */

IMPORT(cpu_info_primary, CPU_INFO_SIZEOF)

#define	GET_CPUINFO		lda v0, cpu_info_primary

#define	GET_CURPROC		lda v0, cpu_info_primary + CPU_INFO_CURPROC

#define	GET_FPCURPROC		lda v0, cpu_info_primary + CPU_INFO_FPCURPROC

#define	GET_CURPCB		lda v0, cpu_info_primary + CPU_INFO_CURPCB

#define	GET_IDLE_PCB(reg)						\
	lda	reg, cpu_info_primary				;	\
	ldq	reg, CPU_INFO_IDLE_PCB_PADDR(reg)
#endif

/*
 * Perform actions necessary to switch to a new context.  The
 * hwpcb should be in a0.  Clobbers v0, t0, t8..t11, a0.
 */
#define	SWITCH_CONTEXT							\
	/* Make a note of the context we're running on. */		\
	GET_CURPCB						;	\
	stq	a0, 0(v0)					;	\
									\
	/* Swap in the new context. */					\
	call_pal PAL_OSF1_swpctx


	/* don't reorder instructions; paranoia. */
	.set noreorder
	.text

	.macro	bfalse	reg, dst
	beq	\reg, \dst
	.endm

	.macro	btrue	reg, dst
	bne	\reg, \dst
	.endm

/*
 * This is for kvm_mkdb, and should be the address of the beginning
 * of the kernel text segment (not necessarily the same as kernbase).
 */
	EXPORT(kernel_text)
.loc	1 __LINE__
kernel_text:

/*
 * bootstack: a temporary stack, for booting.
 *
 * Extends from 'start' down.
 */
bootstack:

/*
 * __start: Kernel start.
 *
 * Arguments:
 *	a0 is the first free page frame number (PFN) (no longer used)
 *	a1 is the page table base register (PTBR)
 *	a2 is the bootinfo magic number
 *	a3 is the pointer to the bootinfo structure
 *
 * All arguments are passed to alpha_init().
 */
NESTED_NOPROFILE(__start,1,0,ra,0,0)
	br	pv,Lstart1
Lstart1: LDGP(pv)

	/* Switch to the boot stack. */
	lda	sp,bootstack

	/* Load KGP with current GP. */
	or	gp,zero,a0
	call_pal PAL_OSF1_wrkgp		/* clobbers a0, t0, t8-t11 */

	/*
	 * Call alpha_init() to do pre-main initialization.
	 * alpha_init() gets the arguments we were called with,
	 * which are already in a0 (destroyed), a1, a2, a3 and a4.
	 */
	CALL(alpha_init)

	/* Set up the virtual page table pointer. */
	ldiq	a0, VPTBASE
	call_pal PAL_OSF1_wrvptptr	/* clobbers a0, t0, t8-t11 */

	/*
	 * Switch to proc0's PCB.
	 */
	lda	a0, proc0
	ldq	a0, P_MD_PCBPADDR(a0)		/* phys addr of PCB */
	SWITCH_CONTEXT

	/*
	 * We've switched to a new page table base, so invalidate the TLB
	 * and I-stream.  This happens automatically everywhere but here.
	 */
	ldiq	a0, -2				/* TBIA */
	call_pal PAL_OSF1_tbi
	call_pal PAL_imb

	/*
	 * All ready to go!  Call main()!
	 */
	CALL(main)

	/* This should never happen. */
	PANIC("main() returned",Lmain_returned_pmsg)
	END(__start)

/**************************************************************************/

/*
 * Pull in the PROM interface routines; these are needed for
 * prom printf (while bootstrapping), and for determining the
 * boot device, etc.
 */
#include <alpha/alpha/prom_disp.s>

/**************************************************************************/

/*
 * Pull in the PALcode function stubs.
 */
#include <alpha/alpha/pal.s>

/**************************************************************************/

/**************************************************************************/

#if defined(MULTIPROCESSOR)
/*
 * Pull in the multiprocessor glue.
 */
#include <alpha/alpha/multiproc.s>
#endif /* MULTIPROCESSOR */

/**************************************************************************/

/**************************************************************************/

#if defined(DDB)
/*
 * Pull in debugger glue.
 */
#include <alpha/alpha/debug.s>
#endif /* DDB */

/**************************************************************************/

/**************************************************************************/

	.text
.loc	1 __LINE__
backtolocore1:
/**************************************************************************/

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 *
 * On entry, stack & registers look like:
 *
 *      a0	signal number
 *      a1	signal specific code
 *      a2	pointer to signal context frame (scp)
 *      a3	address of handler
 *      sp+0	saved hardware state
 *                      .
 *                      .
 *      scp+0	beginning of signal context frame
 */

NESTED(sigcode,0,0,ra,0,0)
	lda	sp, -16(sp)		/* save the sigcontext pointer */
	stq	a2, 0(sp)
	jsr	ra, (t12)		/* call the signal handler (t12==pv) */
	ldq	a0, 0(sp)		/* get the sigcontext pointer */
	lda	sp, 16(sp)
	CALLSYS_NOERROR(sigreturn)	/* and call sigreturn() with it. */
	mov	v0, a0			/* if that failed, get error code */
	CALLSYS_NOERROR(exit)		/* and call exit() with it. */
XNESTED(esigcode,0)
	END(sigcode)

/**************************************************************************/

/*
 * exception_return: return from trap, exception, or syscall
 */

BSS(ssir, 8)

LEAF(exception_return, 1)			/* XXX should be NESTED */
	br	pv, 1f
1:	LDGP(pv)

	ldq	s1, (FRAME_PS * 8)(sp)		/* get the saved PS */
	and	s1, ALPHA_PSL_IPL_MASK, t0	/* look at the saved IPL */
	bne	t0, 4f				/* != 0: can't do AST or SIR */

	/* see if we can do an SIR */
2:	ldq	t1, ssir			/* SIR pending? */
	bne	t1, 5f				/* yes */
	/* no */

	/* check for AST */
3:	and	s1, ALPHA_PSL_USERMODE, t0	/* are we returning to user? */
	beq	t0, 4f				/* no: just return */
	/* yes */

	/* GET_CPUINFO clobbers v0, t0, t8...t11. */
	GET_CPUINFO
	ldq	t1, CPU_INFO_CURPROC(v0)
	ldl	t2, P_MD_ASTPENDING(t1)		/* AST pending? */
	bne	t2, 6f				/* yes */
	/* no: return & deal with FP */

	/*
	 * We are going back to usermode.  Enable the FPU based on whether
	 * the current proc is fpcurproc.  v0 already contains the cpu_info
	 * pointer from above.
	 */
	ldq	t2, CPU_INFO_FPCURPROC(v0)
	cmpeq	t1, t2, t1
	mov	zero, a0
	cmovne	t1, 1, a0
	call_pal PAL_OSF1_wrfen

	/* restore the registers, and return */
4:	bsr	ra, exception_restore_regs	/* jmp/CALL trashes pv/t12 */
	ldq	ra,(FRAME_RA*8)(sp)
	.set noat
	ldq	at_reg,(FRAME_AT*8)(sp)

	lda	sp,(FRAME_SW_SIZE*8)(sp)
	call_pal PAL_OSF1_rti
	.set at
	/* NOTREACHED */

	/* We've got a SIR */
5:	ldiq	a0, ALPHA_PSL_IPL_SOFT
	call_pal PAL_OSF1_swpipl
	mov	v0, s2				/* remember old IPL */
	CALL(softintr_dispatch)

	/* SIR handled; restore IPL and check again */
	mov	s2, a0
	call_pal PAL_OSF1_swpipl
	br	2b

	/* We've got an AST */
6:	ldiq	a0, ALPHA_PSL_IPL_0		/* drop IPL to zero */
	call_pal PAL_OSF1_swpipl
	mov	v0, s2				/* remember old IPL */

	mov	sp, a0				/* only arg is frame */
	CALL(ast)

	/* AST handled; restore IPL and check again */
	mov	s2, a0
	call_pal PAL_OSF1_swpipl
	br	3b

	END(exception_return)

LEAF(exception_save_regs, 0)
	stq	v0,(FRAME_V0*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	t0,(FRAME_T0*8)(sp)
	stq	t1,(FRAME_T1*8)(sp)
	stq	t2,(FRAME_T2*8)(sp)
	stq	t3,(FRAME_T3*8)(sp)
	stq	t4,(FRAME_T4*8)(sp)
	stq	t5,(FRAME_T5*8)(sp)
	stq	t6,(FRAME_T6*8)(sp)
	stq	t7,(FRAME_T7*8)(sp)
	stq	t8,(FRAME_T8*8)(sp)
	stq	t9,(FRAME_T9*8)(sp)
	stq	t10,(FRAME_T10*8)(sp)
	stq	t11,(FRAME_T11*8)(sp)
	stq	t12,(FRAME_T12*8)(sp)
	RET
	END(exception_save_regs)

LEAF(exception_restore_regs, 0)
	ldq	v0,(FRAME_V0*8)(sp)
	ldq	a3,(FRAME_A3*8)(sp)
	ldq	a4,(FRAME_A4*8)(sp)
	ldq	a5,(FRAME_A5*8)(sp)
	ldq	s0,(FRAME_S0*8)(sp)
	ldq	s1,(FRAME_S1*8)(sp)
	ldq	s2,(FRAME_S2*8)(sp)
	ldq	s3,(FRAME_S3*8)(sp)
	ldq	s4,(FRAME_S4*8)(sp)
	ldq	s5,(FRAME_S5*8)(sp)
	ldq	s6,(FRAME_S6*8)(sp)
	ldq	t0,(FRAME_T0*8)(sp)
	ldq	t1,(FRAME_T1*8)(sp)
	ldq	t2,(FRAME_T2*8)(sp)
	ldq	t3,(FRAME_T3*8)(sp)
	ldq	t4,(FRAME_T4*8)(sp)
	ldq	t5,(FRAME_T5*8)(sp)
	ldq	t6,(FRAME_T6*8)(sp)
	ldq	t7,(FRAME_T7*8)(sp)
	ldq	t8,(FRAME_T8*8)(sp)
	ldq	t9,(FRAME_T9*8)(sp)
	ldq	t10,(FRAME_T10*8)(sp)
	ldq	t11,(FRAME_T11*8)(sp)
	ldq	t12,(FRAME_T12*8)(sp)
	RET
	END(exception_restore_regs)

/**************************************************************************/

/*
 * XentArith:
 * System arithmetic trap entry point.
 */

	PALVECT(XentArith)		/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_ARITH
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentArith)

/**************************************************************************/

/*
 * XentIF:
 * System instruction fault trap entry point.
 */

	PALVECT(XentIF)			/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_IF
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)
	jmp	zero, exception_return	
	END(XentIF)

/**************************************************************************/

/*
 * XentInt:
 * System interrupt entry point.
 */

	PALVECT(XentInt)		/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	mov	sp, a3			; .loc 1 __LINE__
	CALL(interrupt)
	jmp	zero, exception_return
	END(XentInt)

/**************************************************************************/

/*
 * XentMM:
 * System memory management fault entry point.
 */

	PALVECT(XentMM)			/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_MM
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentMM)

/**************************************************************************/

/*
 * XentSys:
 * System call entry point.
 */

	ESETUP(XentSys)			; .loc 1 __LINE__

	stq	v0,(FRAME_V0*8)(sp)		/* in case we need to restart */
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	a0,(FRAME_A0*8)(sp)
	stq	a1,(FRAME_A1*8)(sp)
	stq	a2,(FRAME_A2*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	ra,(FRAME_RA*8)(sp)

	/* syscall number, passed in v0, is first arg, frame pointer second */
	mov	v0,a0
	mov	sp,a1			; .loc 1 __LINE__
	CALL(syscall)

	jmp	zero, exception_return
	END(XentSys)

/**************************************************************************/

/*
 * XentUna:
 * System unaligned access entry point.
 */

LEAF(XentUna, 3)				/* XXX should be NESTED */
	.set noat
	lda	sp,-(FRAME_SW_SIZE*8)(sp)
	stq	at_reg,(FRAME_AT*8)(sp)
	.set at
	stq	ra,(FRAME_RA*8)(sp)
	bsr	ra, exception_save_regs		/* jmp/CALL trashes pv/t12 */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_UNA
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentUna)

/**************************************************************************/

/*
 * savefpstate: Save a process's floating point state.
 *
 * Arguments:
 *	a0	'struct fpstate *' to save into
 */

LEAF(savefpstate, 1)
	LDGP(pv)
	/* save all of the FP registers */
	lda	t1, FPREG_FPR_REGS(a0)	/* get address of FP reg. save area */
	stt	$f0,   (0 * 8)(t1)	/* save first register, using hw name */
	stt	$f1,   (1 * 8)(t1)	/* etc. */
	stt	$f2,   (2 * 8)(t1)
	stt	$f3,   (3 * 8)(t1)
	stt	$f4,   (4 * 8)(t1)
	stt	$f5,   (5 * 8)(t1)
	stt	$f6,   (6 * 8)(t1)
	stt	$f7,   (7 * 8)(t1)
	stt	$f8,   (8 * 8)(t1)
	stt	$f9,   (9 * 8)(t1)
	stt	$f10, (10 * 8)(t1)
	stt	$f11, (11 * 8)(t1)
	stt	$f12, (12 * 8)(t1)
	stt	$f13, (13 * 8)(t1)
	stt	$f14, (14 * 8)(t1)
	stt	$f15, (15 * 8)(t1)
	stt	$f16, (16 * 8)(t1)
	stt	$f17, (17 * 8)(t1)
	stt	$f18, (18 * 8)(t1)
	stt	$f19, (19 * 8)(t1)
	stt	$f20, (20 * 8)(t1)
	stt	$f21, (21 * 8)(t1)
	stt	$f22, (22 * 8)(t1)
	stt	$f23, (23 * 8)(t1)
	stt	$f24, (24 * 8)(t1)
	stt	$f25, (25 * 8)(t1)
	stt	$f26, (26 * 8)(t1)
	stt	$f27, (27 * 8)(t1)
	stt	$f28, (28 * 8)(t1)
	stt	$f29, (29 * 8)(t1)
	stt	$f30, (30 * 8)(t1)

	/*
	 * Then save the FPCR; note that the necessary 'trapb's are taken
	 * care of on kernel entry and exit.
	 */
	mf_fpcr	ft0
	stt	ft0, FPREG_FPR_CR(a0)	/* store to FPCR save area */

	RET
	END(savefpstate)

/**************************************************************************/

/*
 * restorefpstate: Restore a process's floating point state.
 *
 * Arguments:
 *	a0	'struct fpstate *' to restore from
 */

LEAF(restorefpstate, 1)
	LDGP(pv)
	/*
	 * Restore the FPCR; note that the necessary 'trapb's are taken care of
	 * on kernel entry and exit.
	 */
	ldt	ft0, FPREG_FPR_CR(a0)	/* load from FPCR save area */
	mt_fpcr	ft0

	/* Restore all of the FP registers. */
	lda	t1, FPREG_FPR_REGS(a0)	/* get address of FP reg. save area */
	ldt	$f0,   (0 * 8)(t1)	/* restore first reg., using hw name */
	ldt	$f1,   (1 * 8)(t1)	/* etc. */
	ldt	$f2,   (2 * 8)(t1)
	ldt	$f3,   (3 * 8)(t1)
	ldt	$f4,   (4 * 8)(t1)
	ldt	$f5,   (5 * 8)(t1)
	ldt	$f6,   (6 * 8)(t1)
	ldt	$f7,   (7 * 8)(t1)
	ldt	$f8,   (8 * 8)(t1)
	ldt	$f9,   (9 * 8)(t1)
	ldt	$f10, (10 * 8)(t1)
	ldt	$f11, (11 * 8)(t1)
	ldt	$f12, (12 * 8)(t1)
	ldt	$f13, (13 * 8)(t1)
	ldt	$f14, (14 * 8)(t1)
	ldt	$f15, (15 * 8)(t1)
	ldt	$f16, (16 * 8)(t1)
	ldt	$f17, (17 * 8)(t1)
	ldt	$f18, (18 * 8)(t1)
	ldt	$f19, (19 * 8)(t1)
	ldt	$f20, (20 * 8)(t1)
	ldt	$f21, (21 * 8)(t1)
	ldt	$f22, (22 * 8)(t1)
	ldt	$f23, (23 * 8)(t1)
	ldt	$f24, (24 * 8)(t1)
	ldt	$f25, (25 * 8)(t1)
	ldt	$f26, (26 * 8)(t1)
	ldt	$f27, (27 * 8)(t1)
	.set noat
	ldt	$f28, (28 * 8)(t1)
	.set at
	ldt	$f29, (29 * 8)(t1)
	ldt	$f30, (30 * 8)(t1)

	RET
	END(restorefpstate)

/**************************************************************************/

/*
 * savectx: save process context, i.e. callee-saved registers
 *
 * Note that savectx() only works for processes other than curproc,
 * since cpu_switch will copy over the info saved here.  (It _can_
 * sanely be used for curproc iff cpu_switch won't be called again, e.g.
 * if called from boot().)
 *
 * Arguments:
 *	a0	'struct user *' of the process that needs its context saved
 *
 * Return:
 *	v0	0.  (note that for child processes, it seems
 *		like savectx() returns 1, because the return address
 *		in the PCB is set to the return address from savectx().)
 */

LEAF(savectx, 1)
	br	pv, 1f
1:	LDGP(pv)
	stq	sp, U_PCB_HWPCB_KSP(a0)		/* store sp */
	stq	s0, U_PCB_CONTEXT+(0 * 8)(a0)	/* store s0 - s6 */
	stq	s1, U_PCB_CONTEXT+(1 * 8)(a0)
	stq	s2, U_PCB_CONTEXT+(2 * 8)(a0)
	stq	s3, U_PCB_CONTEXT+(3 * 8)(a0)
	stq	s4, U_PCB_CONTEXT+(4 * 8)(a0)
	stq	s5, U_PCB_CONTEXT+(5 * 8)(a0)
	stq	s6, U_PCB_CONTEXT+(6 * 8)(a0)
	stq	ra, U_PCB_CONTEXT+(7 * 8)(a0)	/* store ra */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	stq	v0, U_PCB_CONTEXT+(8 * 8)(a0)	/* store ps, for ipl */

	mov	zero, v0
	RET
	END(savectx)

/**************************************************************************/

/*
 * cpu_switchto(struct proc *old, struct proc *new)
 * Switch from "old" proc to "new".
 */
LEAF(cpu_switchto, 2)
	LDGP(pv)

	/*
	 * Don't bother saving the old context if oldproc is NULL.
	 */
	beq	a0, 1f

	/*
	 * do an inline savectx(), to save old context
	 */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	ldq	t0, P_ADDR(a0)
	/* NOTE: ksp is stored by the swpctx */
	stq	s0, U_PCB_CONTEXT+(0 * 8)(t0)	/* store s0 - s6 */
	stq	s1, U_PCB_CONTEXT+(1 * 8)(t0)
	stq	s2, U_PCB_CONTEXT+(2 * 8)(t0)
	stq	s3, U_PCB_CONTEXT+(3 * 8)(t0)
	stq	s4, U_PCB_CONTEXT+(4 * 8)(t0)
	stq	s5, U_PCB_CONTEXT+(5 * 8)(t0)
	stq	s6, U_PCB_CONTEXT+(6 * 8)(t0)
	stq	ra, U_PCB_CONTEXT+(7 * 8)(t0)	/* store ra */
	stq	v0, U_PCB_CONTEXT+(8 * 8)(t0)	/* store ps, for ipl */

1:
	mov	a0, s0				/* save old proc */
	mov	a1, s2				/* save new proc */
	ldq	s3, P_MD_PCBPADDR(s2)		/* save new pcbpaddr */

	/*
	 * Deactivate the old address space before activating the
	 * new one.  We need to do this before activating the
	 * new process's address space in the event that new
	 * process is using the same vmspace as the old.  If we
	 * do this after we activate, then we might end up
	 * incorrectly marking the pmap inactive!
	 *
	 * We don't deactivate if we came here from sched_exit
	 * (old pmap no longer exists; vmspace has been freed).
	 * oldproc will be NULL in this case.  We have actually
	 * taken care of calling pmap_deactivate() in cpu_exit(),
	 * before the vmspace went away.
	 */
	beq	s0, 2f

	mov	s0, a0				/* pmap_deactivate(oldproc) */
	CALL(pmap_deactivate)

2:	/*
	 * Activate the new process's address space and perform
	 * the actual context swap.
	 */

	mov	s2, a0				/* pmap_activate(p) */
	CALL(pmap_activate)

	mov	s3, a0				/* swap the context */
	SWITCH_CONTEXT

	/*
	 * Now that the switch is done, update curproc and other
	 * globals.  We must do this even if switching to ourselves
	 * because we might have re-entered cpu_switch() from idle(),
	 * in which case curproc would be NULL.
	 *
	 * Note: GET_CPUINFO clobbers v0, t0, t8...t11.
	 */
EXPORT(__bwx_switch0)
	addq	s2, P_STAT, t3			/* p->p_stat = SONPROC */
	ldq_u	t1, 0(t3)
	ldiq	t0, SONPROC
	insbl	t0, t3, t0
	mskbl	t1, t3, t1
	or	t0, t1, t0
	stq_u	t0, 0(t3)
EXPORT(__bwx_switch1)

	GET_CPUINFO
	/* p->p_cpu initialized in fork1() for single-processor */
#if defined(MULTIPROCESSOR)
	stq	v0, P_CPU(s2)			/* p->p_cpu = curcpu() */
#endif
	stq	s2, CPU_INFO_CURPROC(v0)	/* curproc = p */

	/*
	 * Now running on the new u struct.
	 * Restore registers and return.
	 */
	ldq	t0, P_ADDR(s2)

	/* NOTE: ksp is restored by the swpctx */
	ldq	s0, U_PCB_CONTEXT+(0 * 8)(t0)		/* restore s0 - s6 */
	ldq	s1, U_PCB_CONTEXT+(1 * 8)(t0)
	ldq	s2, U_PCB_CONTEXT+(2 * 8)(t0)
	ldq	s3, U_PCB_CONTEXT+(3 * 8)(t0)
	ldq	s4, U_PCB_CONTEXT+(4 * 8)(t0)
	ldq	s5, U_PCB_CONTEXT+(5 * 8)(t0)
	ldq	s6, U_PCB_CONTEXT+(6 * 8)(t0)
	ldq	ra, U_PCB_CONTEXT+(7 * 8)(t0)		/* restore ra */
	ldq	a0, U_PCB_CONTEXT+(8 * 8)(t0)		/* restore ipl */
	and	a0, ALPHA_PSL_IPL_MASK, a0
	call_pal PAL_OSF1_swpipl

	ldiq	v0, 1				/* possible ret to savectx() */
	RET
	END(cpu_switchto)

#ifndef SMALL_KERNEL
	/*
	 * BWX-enhanced version of the p->p_stat assignment, to be copied
	 * over the __bwx_switch0 area.

	 * Do not put anything between the end of cpu_switch and this!
	 */
EXPORT(__bwx_switch2)
	ldiq	t0, SONPROC			/* p->p_stat = SONPROC */
	stb	t0, P_STAT(s2)
EXPORT(__bwx_switch3)
#endif

LEAF(cpu_idle_enter, 0)
	RET
	END(cpu_idle_enter)

LEAF(cpu_idle_cycle, 0)
	RET
	END(cpu_idle_cycle)

LEAF(cpu_idle_leave, 0)
	RET
	END(cpu_idle_leave)

/*
 * switch_trampoline()
 *
 * Arrange for a function to be invoked neatly, after a cpu_fork().
 *
 * Invokes the function specified by the s0 register with the return
 * address specified by the s1 register and with one argument specified
 * by the s2 register.
 */
LEAF(switch_trampoline, 0)
#if defined(MULTIPROCESSOR)
	CALL(proc_trampoline_mp)
#endif
	mov	s0, pv
	mov	s1, ra
	mov	s2, a0
	jmp	zero, (pv)
	END(switch_trampoline)

/**************************************************************************/

/*
 * Copy a null-terminated string within the kernel's address space.
 * If lenp is not NULL, store the number of chars copied in *lenp
 *
 * int copystr(char *from, char *to, size_t len, size_t *lenp);
 */
LEAF(copystr, 4)
	LDGP(pv)

	mov	a2, t0			/* t0 = i = len */
	bne	a2, 1f			/* if (len != 0), proceed */
	ldiq	t1, 1			/* else bail */
	br	zero, 2f

1:	ldq_u	t1, 0(a0)		/* t1 = *from */
	extbl	t1, a0, t1
	ldq_u	t3, 0(a1)		/* set up t2 with quad around *to */
	insbl	t1, a1, t2
	mskbl	t3, a1, t3
	or	t3, t2, t3		/* add *from to quad around *to */
	stq_u	t3, 0(a1)		/* write out that quad */

	subl	a2, 1, a2		/* len-- */
	beq	t1, 2f			/* if (*from == 0), bail out */
	addq	a1, 1, a1		/* to++ */
	addq	a0, 1, a0		/* from++ */
	bne	a2, 1b			/* if (len != 0) copy more */

2:	beq	a3, 3f			/* if (lenp != NULL) */
	subl	t0, a2, t0		/* *lenp = (i - len) */
	stq	t0, 0(a3)
3:	beq	t1, 4f			/* *from == '\0'; leave quietly */

	ldiq	v0, ENAMETOOLONG	/* *from != '\0'; error. */
	RET

4:	mov	zero, v0		/* return 0. */
	RET
	END(copystr)

NESTED(copyinstr, 4, 16, ra, IM_RA|IM_S0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	stq	s0, (16-16)(sp)			/* save s0		     */
	ldiq	t0, VM_MAX_ADDRESS		/* make sure that src addr   */
	cmpult	a0, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	/* Note: GET_CURPROC clobbers v0, t0, t8...t11. */
	GET_CURPROC
	mov	v0, s0
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, 0(s0)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(copystr)				/* do the copy.		     */
	.set noat
	ldq	at_reg, 0(s0)			/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	ldq	s0, (16-16)(sp)			/* restore s0.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	RET					/* v0 left over from copystr */
	END(copyinstr)

NESTED(copyoutstr, 4, 16, ra, IM_RA|IM_S0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	stq	s0, (16-16)(sp)			/* save s0		     */
	ldiq	t0, VM_MAX_ADDRESS		/* make sure that dest addr  */
	cmpult	a1, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	/* Note: GET_CURPROC clobbers v0, t0, t8...t11. */
	GET_CURPROC
	mov	v0, s0
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, 0(s0)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(copystr)				/* do the copy.		     */
	.set noat
	ldq	at_reg, 0(s0)			/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	ldq	s0, (16-16)(sp)			/* restore s0.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	RET					/* v0 left over from copystr */
	END(copyoutstr)

/*
 * kcopy(const void *src, void *dst, size_t len);
 *
 * Copy len bytes from src to dst, aborting if we encounter a fatal
 * page fault.
 *
 * kcopy() _must_ save and restore the old fault handler since it is
 * called by uiomove(), which may be in the path of servicing a non-fatal
 * page fault.
 */
NESTED(kcopy, 3, 32, ra, IM_RA|IM_S0|IM_S1, 0)
	LDGP(pv)
	lda	sp, -32(sp)			/* set up stack frame	     */
	stq	ra, (32-8)(sp)			/* save ra		     */
	stq	s0, (32-16)(sp)			/* save s0		     */
	stq	s1, (32-24)(sp)			/* save s1		     */
	/* Note: GET_CURPROC clobbers v0, t0, t8...t11. */
	GET_CURPROC
	mov	v0, s1
	lda	v0, kcopyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, 0(s1)
	ldq	at_reg, P_ADDR(at_reg)
	ldq	s0, U_PCB_ONFAULT(at_reg)	/* save old handler.	     */
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(bcopy)				/* do the copy.		     */
	.set noat
	ldq	at_reg, 0(s1)			/* restore the old handler.  */
	ldq	at_reg, P_ADDR(at_reg)
	stq	s0, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (32-8)(sp)			/* restore ra.		     */
	ldq	s0, (32-16)(sp)			/* restore s0.		     */
	ldq	s1, (32-24)(sp)			/* restore s1.		     */
	lda	sp, 32(sp)			/* kill stack frame.	     */
	mov	zero, v0			/* return 0. */
	RET
	END(kcopy)

LEAF(kcopyerr, 0)
	LDGP(pv)
	.set noat
	ldq	at_reg, 0(s1)			/* restore the old handler.  */
	ldq	at_reg, P_ADDR(at_reg)
	stq	s0, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (32-8)(sp)			/* restore ra.		     */
	ldq	s0, (32-16)(sp)			/* restore s0.		     */
	ldq	s1, (32-24)(sp)			/* restore s1.		     */
	lda	sp, 32(sp)			/* kill stack frame.	     */
	ldiq	v0, EFAULT			/* return EFAULT.	     */
	RET
END(kcopyerr)

NESTED(copyin, 3, 16, ra, IM_RA|IM_S0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	stq	s0, (16-16)(sp)			/* save s0		     */
	ldiq	t0, VM_MAX_ADDRESS		/* make sure that src addr   */
	cmpult	a0, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	/* Note: GET_CURPROC clobbers v0, t0, t8...t11. */
	GET_CURPROC
	mov	v0, s0
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, 0(s0)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(bcopy)				/* do the copy.		     */
	.set noat
	ldq	at_reg, 0(s0)			/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	ldq	s0, (16-16)(sp)			/* restore s0.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	mov	zero, v0			/* return 0. */
	RET
	END(copyin)

NESTED(copyout, 3, 16, ra, IM_RA|IM_S0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	stq	s0, (16-16)(sp)			/* save s0		     */
	ldiq	t0, VM_MAX_ADDRESS		/* make sure that dest addr  */
	cmpult	a1, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	/* Note: GET_CURPROC clobbers v0, t0, t8...t11. */
	GET_CURPROC
	mov	v0, s0
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, 0(s0)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(bcopy)				/* do the copy.		     */
	.set noat
	ldq	at_reg, 0(s0)			/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	ldq	s0, (16-16)(sp)			/* restore s0.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	mov	zero, v0			/* return 0. */
	RET
	END(copyout)

LEAF(copyerr, 0)
	LDGP(pv)
	.set noat
	ldq	at_reg, P_ADDR(at_reg)		/* clear handler.            */
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	ldq	s0, (16-16)(sp)			/* restore s0.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	ldiq	v0, EFAULT			/* return EFAULT.	     */
	RET
END(copyerr)

/**************************************************************************/

/*
 * console 'restart' routine to be placed in HWRPB.
 */
LEAF(XentRestart, 1)			/* XXX should be NESTED */
	.set noat
	lda	sp,-(FRAME_SIZE*8)(sp)
	stq	at_reg,(FRAME_AT*8)(sp)
	.set at
	stq	v0,(FRAME_V0*8)(sp)
	stq	a0,(FRAME_A0*8)(sp)
	stq	a1,(FRAME_A1*8)(sp)
	stq	a2,(FRAME_A2*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	t0,(FRAME_T0*8)(sp)
	stq	t1,(FRAME_T1*8)(sp)
	stq	t2,(FRAME_T2*8)(sp)
	stq	t3,(FRAME_T3*8)(sp)
	stq	t4,(FRAME_T4*8)(sp)
	stq	t5,(FRAME_T5*8)(sp)
	stq	t6,(FRAME_T6*8)(sp)
	stq	t7,(FRAME_T7*8)(sp)
	stq	t8,(FRAME_T8*8)(sp)
	stq	t9,(FRAME_T9*8)(sp)
	stq	t10,(FRAME_T10*8)(sp)
	stq	t11,(FRAME_T11*8)(sp)
	stq	t12,(FRAME_T12*8)(sp)
	stq	ra,(FRAME_RA*8)(sp)

	br	pv,1f
1:	LDGP(pv)

	mov	sp,a0
	CALL(console_restart)

	call_pal PAL_halt
	END(XentRestart)

/**************************************************************************/

/*
 * Kernel setjmp and longjmp.  Rather minimalist.
 *
 *	longjmp(label_t *a)
 * will generate a "return (1)" from the last call to
 *	setjmp(label_t *a)
 * by restoring registers from the stack,
 */

	.set	noreorder

LEAF(setjmp, 1)
	LDGP(pv)

	stq	ra, (0 * 8)(a0)			/* return address */
	stq	s0, (1 * 8)(a0)			/* callee-saved registers */
	stq	s1, (2 * 8)(a0)
	stq	s2, (3 * 8)(a0)
	stq	s3, (4 * 8)(a0)
	stq	s4, (5 * 8)(a0)
	stq	s5, (6 * 8)(a0)
	stq	s6, (7 * 8)(a0)
	stq	sp, (8 * 8)(a0)

	ldiq	t0, 0xbeeffedadeadbabe		/* set magic number */
	stq	t0, (9 * 8)(a0)

	mov	zero, v0			/* return zero */
	RET
END(setjmp)

LEAF(longjmp, 1)
	LDGP(pv)

	ldiq	t0, 0xbeeffedadeadbabe		/* check magic number */
	ldq	t1, (9 * 8)(a0)
	cmpeq	t0, t1, t0
	beq	t0, longjmp_botch		/* if bad, punt */

	ldq	ra, (0 * 8)(a0)			/* return address */
	ldq	s0, (1 * 8)(a0)			/* callee-saved registers */
	ldq	s1, (2 * 8)(a0)
	ldq	s2, (3 * 8)(a0)
	ldq	s3, (4 * 8)(a0)
	ldq	s4, (5 * 8)(a0)
	ldq	s5, (6 * 8)(a0)
	ldq	s6, (7 * 8)(a0)
	ldq	sp, (8 * 8)(a0)

	ldiq	v0, 1
	RET

longjmp_botch:
	lda	a0, longjmp_botchmsg
	mov	ra, a1
	CALL(panic)
	call_pal PAL_bugchk

	.data
longjmp_botchmsg:
	.asciz	"longjmp botch from %p"
	.text
END(longjmp)

/*
 * void sts(int rn, u_int32_t *rval);
 * void stt(int rn, u_int64_t *rval);
 * void lds(int rn, u_int32_t *rval);
 * void ldt(int rn, u_int64_t *rval);
 */

#ifndef NO_IEEE
.macro make_freg_util name, op
	LEAF(alpha_\name, 2)
	and	a0, 0x1f, a0
	s8addq	a0, pv, pv
	addq	pv, 1f - alpha_\name, pv
	jmp	(pv)
1:
	rn = 0
	.rept   32
	\op     $f0 + rn, 0(a1)
	RET
	rn = rn + 1
	.endr
	END(alpha_\name)
.endm
/*
LEAF(alpha_sts, 2)
LEAF(alpha_stt, 2)
LEAF(alpha_lds, 2)
LEAF(alpha_ldt, 2)
 */
	make_freg_util sts, sts
	make_freg_util stt, stt
	make_freg_util lds, lds
	make_freg_util ldt, ldt

LEAF(alpha_read_fpcr, 0); f30save = 0; rettmp = 8; framesz = 16
	lda	sp, -framesz(sp)
	stt	$f30, f30save(sp)
	mf_fpcr	$f30
	stt	$f30, rettmp(sp)
	ldt	$f30, f30save(sp)
	ldq	v0, rettmp(sp)
	lda	sp, framesz(sp)
	RET
END(alpha_read_fpcr)

LEAF(alpha_write_fpcr, 1); f30save = 0; fpcrtmp = 8; framesz = 16
	lda	sp, -framesz(sp)
	stq	a0, fpcrtmp(sp)
	stt	$f30, f30save(sp)
	ldt	$f30, fpcrtmp(sp)
	mt_fpcr	$f30
	ldt	$f30, f30save(sp)
	lda	sp, framesz(sp)
	RET
END(alpha_write_fpcr)
#endif

#if 0
NESTED(transfer_check,0,0,ra,0,0)
	CALL(U_need_2_run_config)
	END(transfer_check)
#endif

/* Random data that shouldn't be necessary. */
	.data
EXPORT(cold)
	.long 1			/* cold start flag (.long -> _4_ bytes) */
	.align 3
EXPORT(esym)
	.quad 1			/* store end of kernel symbol table here */


/**************************************************************************/
