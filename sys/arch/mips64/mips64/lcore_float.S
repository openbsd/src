/*	$OpenBSD: lcore_float.S,v 1.7 2004/09/27 19:16:06 pefo Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>
#include <machine/pte.h>

#include "assym.h"

	.set	noreorder		# Noreorder is default style!

	.set	mips3

/*----------------------------------------------------------------------------
 *
 * MipsSwitchFPState --
 *
 *	Save the current state into 'from' and restore it from 'to'.
 *
 *	MipsSwitchFPState(from, to)
 *		struct proc *from;
 *		struct user *to;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
LEAF(MipsSwitchFPState, 0)
	mfc0	t1, COP_0_STATUS_REG	# Save old SR
	li	t0, SR_COP_1_BIT|SR_FR_32	# enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	beq	a0, zero, 1f		# skip save if NULL pointer
	nop
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	PTR_L	a0, P_ADDR(a0)		# get pointer to pcb for proc
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
	li	t3, ~SR_COP_1_BIT
	REG_L	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# get CPU status register
	REG_S	t0, U_PCB_FPREGS+(32 * REGSZ)(a0)	# save FP status
	and	t2, t2, t3			# clear COP_1 enable bit
	REG_S	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# save new status register
/*
 * Save the floating point registers.
 */
	sdc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a0)
	sdc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a0)
	sdc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a0)
	sdc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a0)
	sdc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a0)
	sdc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a0)
	sdc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a0)
	sdc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a0)
	sdc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a0)
	sdc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a0)
	sdc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a0)
	sdc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a0)
	sdc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a0)
	sdc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a0)
	sdc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a0)
	sdc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a0)
	sdc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a0)
	sdc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a0)
	sdc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a0)
	sdc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a0)
	sdc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a0)
	sdc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a0)
	sdc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a0)
	sdc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a0)
	sdc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a0)
	sdc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a0)
	sdc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a0)
	sdc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a0)
	sdc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a0)
	sdc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a0)
	sdc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a0)
	sdc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a0)

1:
/*
 *  Restore the floating point registers.
 */
	REG_L	t0, U_PCB_FPREGS+(32 * REGSZ)(a1)	# get status register
	ldc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a1)
	ldc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a1)
	ldc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a1)
	ldc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a1)
	ldc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a1)
	ldc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a1)
	ldc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a1)
	ldc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a1)
	ldc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a1)
	ldc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a1)
	ldc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a1)
	ldc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a1)
	ldc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a1)
	ldc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a1)
	ldc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a1)
	ldc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a1)
	ldc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a1)
	ldc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a1)
	ldc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a1)
	ldc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a1)
	ldc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a1)
	ldc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a1)
	ldc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a1)
	ldc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a1)
	ldc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a1)
	ldc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a1)
	ldc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a1)
	ldc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a1)
	ldc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a1)
	ldc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a1)
	ldc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a1)
	ldc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a1)

	and	t0, t0, ~FPC_EXCEPTION_BITS
	ctc1	t0, FPC_CSR
	nop

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	ITLBNOPFIX
	j	ra
	nop
END(MipsSwitchFPState)

LEAF(MipsSwitchFPState16, 0)
	mfc0	t1, COP_0_STATUS_REG	# Save old SR
	li	t0, SR_COP_1_BIT	# enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	beq	a0, zero, 1f		# skip save if NULL pointer
	nop
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	PTR_L	a0, P_ADDR(a0)		# get pointer to pcb for proc
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
	li	t3, ~SR_COP_1_BIT
	REG_L	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# get CPU status register
	REG_S	t0, U_PCB_FPREGS+(32 * REGSZ)(a0)	# save FP status
	and	t2, t2, t3			# clear COP_1 enable bit
	REG_S	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# save new status register
/*
 * Save the floating point registers.
 */
	swc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a0)
	swc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a0)
	swc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a0)
	swc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a0)
	swc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a0)
	swc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a0)
	swc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a0)
	swc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a0)
	swc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a0)
	swc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a0)
	swc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a0)
	swc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a0)
	swc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a0)
	swc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a0)
	swc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a0)
	swc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a0)
	swc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a0)
	swc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a0)
	swc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a0)
	swc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a0)
	swc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a0)
	swc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a0)
	swc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a0)
	swc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a0)
	swc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a0)
	swc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a0)
	swc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a0)
	swc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a0)
	swc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a0)
	swc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a0)
	swc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a0)
	swc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a0)

1:
/*
 *  Restore the floating point registers.
 */
	REG_L	t0, U_PCB_FPREGS+(32 * REGSZ)(a1)	# get status register
	lwc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a1)
	lwc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a1)
	lwc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a1)
	lwc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a1)
	lwc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a1)
	lwc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a1)
	lwc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a1)
	lwc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a1)
	lwc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a1)
	lwc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a1)
	lwc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a1)
	lwc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a1)
	lwc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a1)
	lwc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a1)
	lwc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a1)
	lwc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a1)
	lwc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a1)
	lwc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a1)
	lwc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a1)
	lwc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a1)
	lwc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a1)
	lwc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a1)
	lwc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a1)
	lwc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a1)
	lwc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a1)
	lwc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a1)
	lwc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a1)
	lwc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a1)
	lwc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a1)
	lwc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a1)
	lwc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a1)
	lwc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a1)

	and	t0, t0, ~FPC_EXCEPTION_BITS
	ctc1	t0, FPC_CSR
	nop

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	ITLBNOPFIX
	j	ra
	nop
END(MipsSwitchFPState16)

/*----------------------------------------------------------------------------
 *
 * MipsSaveCurFPState --
 *
 *	Save the current floating point coprocessor state.
 *
 *	MipsSaveCurFPState(p)
 *		struct proc *p;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	machFPCurProcPtr is cleared.
 *
 *----------------------------------------------------------------------------
 */
LEAF(MipsSaveCurFPState, 0)
	PTR_L	a0, P_ADDR(a0)			# get pointer to pcb for proc
	mfc0	t1, COP_0_STATUS_REG	# Disable interrupts and
	li	t0, SR_COP_1_BIT|SR_FR_32	# enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	PTR_S	zero, machFPCurProcPtr		# indicate state has been saved
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	REG_L	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# get CPU status register
	li	t3, ~SR_COP_1_BIT
	and	t2, t2, t3			# clear COP_1 enable bit
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
	REG_S	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# save new status register
	REG_S	t0, U_PCB_FPREGS+(32 * REGSZ)(a0)	# save FP status
/*
 * Save the floating point registers.
 */
	sdc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a0)
	sdc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a0)
	sdc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a0)
	sdc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a0)
	sdc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a0)
	sdc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a0)
	sdc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a0)
	sdc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a0)
	sdc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a0)
	sdc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a0)
	sdc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a0)
	sdc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a0)
	sdc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a0)
	sdc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a0)
	sdc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a0)
	sdc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a0)
	sdc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a0)
	sdc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a0)
	sdc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a0)
	sdc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a0)
	sdc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a0)
	sdc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a0)
	sdc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a0)
	sdc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a0)
	sdc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a0)
	sdc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a0)
	sdc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a0)
	sdc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a0)
	sdc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a0)
	sdc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a0)
	sdc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a0)
	sdc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a0)

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	ITLBNOPFIX
	j	ra
	nop
END(MipsSaveCurFPState)

LEAF(MipsSaveCurFPState16, 0)
	PTR_L	a0, P_ADDR(a0)			# get pointer to pcb for proc
	mfc0	t1, COP_0_STATUS_REG	# Disable interrupts and
	li	t0, SR_COP_1_BIT	# enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	PTR_S	zero, machFPCurProcPtr		# indicate state has been saved
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	REG_L	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# get CPU status register
	li	t3, ~SR_COP_1_BIT
	and	t2, t2, t3			# clear COP_1 enable bit
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
	REG_S	t2, U_PCB_REGS+(PS * REGSZ)(a0)	# save new status register
	REG_S	t0, U_PCB_FPREGS+(32 * REGSZ)(a0)	# save FP status
/*
 * Save the floating point registers.
 */
	swc1	$f0, U_PCB_FPREGS+(0 * REGSZ)(a0)
	swc1	$f1, U_PCB_FPREGS+(1 * REGSZ)(a0)
	swc1	$f2, U_PCB_FPREGS+(2 * REGSZ)(a0)
	swc1	$f3, U_PCB_FPREGS+(3 * REGSZ)(a0)
	swc1	$f4, U_PCB_FPREGS+(4 * REGSZ)(a0)
	swc1	$f5, U_PCB_FPREGS+(5 * REGSZ)(a0)
	swc1	$f6, U_PCB_FPREGS+(6 * REGSZ)(a0)
	swc1	$f7, U_PCB_FPREGS+(7 * REGSZ)(a0)
	swc1	$f8, U_PCB_FPREGS+(8 * REGSZ)(a0)
	swc1	$f9, U_PCB_FPREGS+(9 * REGSZ)(a0)
	swc1	$f10, U_PCB_FPREGS+(10 * REGSZ)(a0)
	swc1	$f11, U_PCB_FPREGS+(11 * REGSZ)(a0)
	swc1	$f12, U_PCB_FPREGS+(12 * REGSZ)(a0)
	swc1	$f13, U_PCB_FPREGS+(13 * REGSZ)(a0)
	swc1	$f14, U_PCB_FPREGS+(14 * REGSZ)(a0)
	swc1	$f15, U_PCB_FPREGS+(15 * REGSZ)(a0)
	swc1	$f16, U_PCB_FPREGS+(16 * REGSZ)(a0)
	swc1	$f17, U_PCB_FPREGS+(17 * REGSZ)(a0)
	swc1	$f18, U_PCB_FPREGS+(18 * REGSZ)(a0)
	swc1	$f19, U_PCB_FPREGS+(19 * REGSZ)(a0)
	swc1	$f20, U_PCB_FPREGS+(20 * REGSZ)(a0)
	swc1	$f21, U_PCB_FPREGS+(21 * REGSZ)(a0)
	swc1	$f22, U_PCB_FPREGS+(22 * REGSZ)(a0)
	swc1	$f23, U_PCB_FPREGS+(23 * REGSZ)(a0)
	swc1	$f24, U_PCB_FPREGS+(24 * REGSZ)(a0)
	swc1	$f25, U_PCB_FPREGS+(25 * REGSZ)(a0)
	swc1	$f26, U_PCB_FPREGS+(26 * REGSZ)(a0)
	swc1	$f27, U_PCB_FPREGS+(27 * REGSZ)(a0)
	swc1	$f28, U_PCB_FPREGS+(28 * REGSZ)(a0)
	swc1	$f29, U_PCB_FPREGS+(29 * REGSZ)(a0)
	swc1	$f30, U_PCB_FPREGS+(30 * REGSZ)(a0)
	swc1	$f31, U_PCB_FPREGS+(31 * REGSZ)(a0)

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	ITLBNOPFIX
	j	ra
	nop
END(MipsSaveCurFPState16)

/*----------------------------------------------------------------------------
 *
 * MipsFPTrap --
 *
 *	Handle a floating point Trap.
 *
 *	MipsFPTrap(statusReg, causeReg, pc)
 *		unsigned statusReg;
 *		unsigned causeReg;
 *		unsigned pc;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(MipsFPTrap, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	mfc0	t0, COP_0_STATUS_REG
	PTR_S	ra, CF_RA_OFFS(sp)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))

	or	t1, t0, SR_COP_1_BIT
	mtc0	t1, COP_0_STATUS_REG
	ITLBNOPFIX
	cfc1	t1, FPC_CSR	# stall til FP done
	cfc1	t1, FPC_CSR	# now get status
	nop
	sll	t2, t1, (31 - 17)	# unimplemented operation?
	bgez	t2, 3f			# no, normal trap
	nop
/*
 * We got an unimplemented operation trap so
 * fetch the instruction, compute the next PC and emulate the instruction.
 */
	bgez	a1, 1f			# Check the branch delay bit.
	nop
/*
 * The instruction is in the branch delay slot so the branch will have to
 * be emulated to get the resulting PC.
 */
	PTR_S	a2, FRAMESZ(CF_SZ) + 2 * REGSZ(sp)
	PTR_L	a0, curprocpaddr		# first arg is ptr to CPU regs
	move	a1, a2				# second arg is instruction PC
	move	a2, t1				# third arg is floating point CSR
	jal	MipsEmulateBranch		# compute PC after branch
	move	a3, zero			# fourth arg is FALSE
/*
 * Now load the floating-point instruction in the branch delay slot
 * to be emulated.
 */
	PTR_L	a2, FRAMESZ(CF_SZ) + 2 * REGSZ(sp)	# restore EXC pc
	b	2f
	lw	a0, 4(a2)			# a0 = coproc instruction
/*
 * This is not in the branch delay slot so calculate the resulting
 * PC (epc + 4) into v0 and continue to MipsEmulateFP().
 */
1:
	lw	a0, 0(a2)			# a0 = coproc instruction
	addu	v0, a2, 4			# v0 = next pc
2:
	PTR_L	a3, curprocpaddr		# first arg is ptr to CPU regs
	PTR_S	v0, U_PCB_REGS+(PC * REGSZ)(a3)	# save new pc
/*
 * Check to see if the instruction to be emulated is a floating-point
 * instruction.
 */
	srl	a3, a0, OPCODE_SHIFT
	beq	a3, OPCODE_C1, 4f		# this should never fail
	nop
/*
 * Send a floating point exception signal to the current process.
 */
3:
	PTR_L	a0, curproc		# get current process
	cfc1	a2, FPC_CSR		# code = FP execptions
	ctc1	zero, FPC_CSR		# Clear exceptions
	jal	trapsignal
	li	a1, SIGFPE
	b	FPReturn
	nop

/*
 * Finally, we can call MipsEmulateFP() where a0 is the instruction to emulate.
 */
4:
	jal	MipsEmulateFP
	nop

	bnez	v0, 3b			# Emulation failed.
	nop

/*
 * Turn off the floating point coprocessor and return.
 */
FPReturn:
	mfc0	t0, COP_0_STATUS_REG
	PTR_L	ra, CF_RA_OFFS(sp)
	and	t0, t0, ~SR_COP_1_BIT
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	j	ra
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
END(MipsFPTrap)

/*----------------------------------------------------------------------------
 *
 * cp1_get_prid
 *
 *	Get the floating point co-processor id.
 *
 *	cp1_get_prid(void)
 *
 * Results:
 *	FPC_ID
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
LEAF(cp1_get_prid, 0)
	mfc0	v1, COP_0_STATUS_REG
	li	a0, SR_COP_1_BIT
	or	v1, a0
	mtc0	v1, COP_0_STATUS_REG
	nop
	nop
	nop
	nop
	cfc1	v0, FPC_ID
	xor	v1, a0
	mtc0	v1, COP_0_STATUS_REG
	ITLBNOPFIX
	jr	ra
	nop
END(cp1_get_prid)

