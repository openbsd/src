/*	$NetBSD: locore_r2000.S,v 1.27 1996/10/13 21:37:41 jonathan Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *
 *	@(#)locore.s	8.5 (Berkeley) 1/4/94
 */

/*
 *----------------------------------------------------------------------------
 *
 * mips1_UTLBmiss --
 *
 *	Vector code for a MIPS-I user-space TLB miss from user-space.
 *
 *
 * This code is copied to the UTLB exception vector address to
 * handle user level TLB translation misses.
 * NOTE: This code must be relocatable!!!
 */
	.globl	_C_LABEL(mips1_UTLBMiss)
_C_LABEL(mips1_UTLBMiss):
	.set	noat
	mfc0	k0, MIPS_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, UADDR+U_PCB_SEGTAB		# get the current segment table
	bltz	k0, 1f				# R3000 chip bug
	srl	k0, k0, SEGSHIFT		# compute segment table index
	sll	k0, k0, 2
	addu	k1, k1, k0
	mfc0	k0, MIPS_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, 0(k1)			# get pointer to segment map
	srl	k0, k0, PGSHIFT - 2		# compute segment map index
	andi	k0, k0, (NPTEPG - 1) << 2
	beq	k1, zero, 2f			# invalid segment map
	addu	k1, k1, k0			# index into segment map
	lw	k0, 0(k1)			# get page PTE
	nop
	beq	k0, zero, 2f			# dont load invalid entries
	mtc0	k0, MIPS_COP_0_TLB_LOW
	mfc0	k1, MIPS_COP_0_EXC_PC		# get return address
	tlbwr					# update TLB
	j	k1
	rfe
1:
	mfc0	k1, MIPS_COP_0_EXC_PC		# get return address
	nop
	j	k1
	rfe
2:
	j	mips1_SlowFault			# handle the rest
	nop
	.set	at
	.globl	_C_LABEL(mips1_UTLBMissEnd)
_C_LABEL(mips1_UTLBMissEnd):


/*
 *----------------------------------------------------------------------------
 *
 * mips1_execption --
 *
 *	Vector code for the general exception vector 0x80000080
 *	on an r2000 or r3000.
 *
 * This code is copied to the general exception vector address to
 * handle all execptions except RESET and UTLBMiss.
 * NOTE: This code must be relocatable!!!
 *
 *----------------------------------------------------------------------------
 */
	.globl	_C_LABEL(mips1_exception)
_C_LABEL(mips1_exception):
/*
 * Find out what mode we came from and jump to the proper handler.
 */
	.set	noat
	mfc0	k0, MIPS_COP_0_STATUS_REG	# Get the status register
	mfc0	k1, MIPS_COP_0_CAUSE_REG	# Get the cause register value.
	and	k0, k0, MIPS1_SR_KU_PREV	# test for user mode
	sll	k0, k0, 4			# shift user bit for cause index
	and	k1, k1, MIPS1_CR_EXC_CODE	# Mask out the cause bits.
	or	k1, k1, k0			# change index to user table
1:
	la	k0, _C_LABEL(mips1_ExceptionTable) # get base of the jump table
	addu	k0, k0, k1			# Get the address of the
						#  function entry.  Note that
						#  the cause is already
						#  shifted left by 2 bits so
						#  we dont have to shift.
	lw	k0, 0(k0)			# Get the function address
	nop
	j	k0				# Jump to the function.
	nop
	.set	at
	.globl	_C_LABEL(mips1_exceptionEnd)
_C_LABEL(mips1_exceptionEnd):




/*----------------------------------------------------------------------------
 *
 * mips1_SlowFault --
 *
 * Alternate entry point into the mips1_UserGenExceptionor or
 * or mips1_user_Kern_exception, when the ULTB miss handler couldn't
 * find a TLB entry.
 *
 * Find out what mode we came from and call the appropriate handler.
 */
mips1_SlowFault:
	.set	noat
	mfc0	k0, MIPS_COP_0_STATUS_REG
	nop
	and	k0, k0, MIPS_SR_KU_PREV
	bne	k0, zero, _C_LABEL(mips1_UserGenException)
	nop
	.set	at
/*
 * Fall though ...
 */

/*----------------------------------------------------------------------------
 *
 * mips1_KernGenException --
 *
 *	Handle an exception from kernel mode.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */

/*
 * The kernel exception stack contains 18 saved general registers,
 * the status register and the multiply lo and high registers.
 * In addition, we set this up for linkage conventions.
 */
#define KERN_REG_SIZE		(18 * 4)
#define KERN_REG_OFFSET		(STAND_FRAME_SIZE)
#define KERN_SR_OFFSET		(STAND_FRAME_SIZE + KERN_REG_SIZE)
#define KERN_MULT_LO_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 4)
#define KERN_MULT_HI_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 8)
#define	KERN_EXC_FRAME_SIZE	(STAND_FRAME_SIZE + KERN_REG_SIZE + 12)

NNON_LEAF(mips1_KernGenException, KERN_EXC_FRAME_SIZE, ra)
	.set	noat
#ifdef KADB
	la	k0, kdbpcb			# save registers for kadb
	sw	s0, (S0 * 4)(k0)
	sw	s1, (S1 * 4)(k0)
	sw	s2, (S2 * 4)(k0)
	sw	s3, (S3 * 4)(k0)
	sw	s4, (S4 * 4)(k0)
	sw	s5, (S5 * 4)(k0)
	sw	s6, (S6 * 4)(k0)
	sw	s7, (S7 * 4)(k0)
	sw	s8, (S8 * 4)(k0)
	sw	gp, (GP * 4)(k0)
	sw	sp, (SP * 4)(k0)
#endif
	subu	sp, sp, KERN_EXC_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - KERN_EXC_FRAME_SIZE)
/*
 * Save the relevant kernel registers onto the stack.
 * We don't need to save s0 - s8, sp and gp because
 * the compiler does it for us.
 */
	sw	AT, KERN_REG_OFFSET + 0(sp)
	sw	v0, KERN_REG_OFFSET + 4(sp)
	sw	v1, KERN_REG_OFFSET + 8(sp)
	sw	a0, KERN_REG_OFFSET + 12(sp)
	mflo	v0
	mfhi	v1
	sw	a1, KERN_REG_OFFSET + 16(sp)
	sw	a2, KERN_REG_OFFSET + 20(sp)
	sw	a3, KERN_REG_OFFSET + 24(sp)
	sw	t0, KERN_REG_OFFSET + 28(sp)
	mfc0	a0, MIPS_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, KERN_REG_OFFSET + 32(sp)
	sw	t2, KERN_REG_OFFSET + 36(sp)
	sw	t3, KERN_REG_OFFSET + 40(sp)
	sw	t4, KERN_REG_OFFSET + 44(sp)
	mfc0	a1, MIPS_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, KERN_REG_OFFSET + 48(sp)
	sw	t6, KERN_REG_OFFSET + 52(sp)
	sw	t7, KERN_REG_OFFSET + 56(sp)
	sw	t8, KERN_REG_OFFSET + 60(sp)
	mfc0	a2, MIPS_COP_0_BAD_VADDR	# Third arg is the fault addr.
	sw	t9, KERN_REG_OFFSET + 64(sp)
	sw	ra, KERN_REG_OFFSET + 68(sp)
	sw	v0, KERN_MULT_LO_OFFSET(sp)
	sw	v1, KERN_MULT_HI_OFFSET(sp)
	mfc0	a3, MIPS_COP_0_EXC_PC		# Fourth arg is the pc.
	sw	a0, KERN_SR_OFFSET(sp)
/*
 * Call the exception handler.
 */
	jal	_C_LABEL(trap)
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the exception.
 * v0 contains the return address.
 */
	lw	a0, KERN_SR_OFFSET(sp)
	lw	t0, KERN_MULT_LO_OFFSET(sp)
	lw	t1, KERN_MULT_HI_OFFSET(sp)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	move	k0, v0

	lw	AT, KERN_REG_OFFSET + 0(sp)
	lw	v0, KERN_REG_OFFSET + 4(sp)

	RESTORE_KERN_REGISTERS(KERN_REG_OFFSET)

	addu	sp, sp, KERN_EXC_FRAME_SIZE
	j	k0				# Now return from the
	rfe					#  exception.
	.set	at
END(mips1_KernGenException)

/*----------------------------------------------------------------------------
 *
 * mips1_UserGenException --
 *
 *	Handle an exception from user mode.
 *
 * Results:
 * 	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
NNON_LEAF(mips1_UserGenException, STAND_FRAME_SIZE, ra)
	.set	noat
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
/*
 * Save all of the registers except for the kernel temporaries in u.u_pcb.
 */
	sw	AT, UADDR+U_PCB_REGS+(AST * 4)
	sw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	sw	v1, UADDR+U_PCB_REGS+(V1 * 4)
	sw	a0, UADDR+U_PCB_REGS+(A0 * 4)
	mflo	v0
	sw	a1, UADDR+U_PCB_REGS+(A1 * 4)
	sw	a2, UADDR+U_PCB_REGS+(A2 * 4)
	sw	a3, UADDR+U_PCB_REGS+(A3 * 4)
	sw	t0, UADDR+U_PCB_REGS+(T0 * 4)
	mfhi	v1
	sw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	sw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	sw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	sw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	mfc0	a0, MIPS_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	s0, UADDR+U_PCB_REGS+(S0 * 4)
	mfc0	a1, MIPS_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	s1, UADDR+U_PCB_REGS+(S1 * 4)
	sw	s2, UADDR+U_PCB_REGS+(S2 * 4)
	sw	s3, UADDR+U_PCB_REGS+(S3 * 4)
	sw	s4, UADDR+U_PCB_REGS+(S4 * 4)
	mfc0	a2, MIPS_COP_0_BAD_VADDR	# Third arg is the fault addr
	sw	s5, UADDR+U_PCB_REGS+(S5 * 4)
	sw	s6, UADDR+U_PCB_REGS+(S6 * 4)
	sw	s7, UADDR+U_PCB_REGS+(S7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a3, MIPS_COP_0_EXC_PC		# Fourth arg is the pc.
	sw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	sw	gp, UADDR+U_PCB_REGS+(GP * 4)
	sw	sp, UADDR+U_PCB_REGS+(SP * 4)
	sw	s8, UADDR+U_PCB_REGS+(S8 * 4)
	li	sp, KERNELSTACK - STAND_FRAME_SIZE	# switch to kernel SP
	sw	ra, UADDR+U_PCB_REGS+(RA * 4)
	sw	v0, UADDR+U_PCB_REGS+(MULLO * 4)
	sw	v1, UADDR+U_PCB_REGS+(MULHI * 4)
	sw	a0, UADDR+U_PCB_REGS+(SR * 4)
#ifdef __GP_SUPPORT__
	la	gp, _C_LABEL(_gp)		# switch to kernel GP
#endif
	sw	a3, UADDR+U_PCB_REGS+(PC * 4)
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
	.set	at
	and	t0, a0, ~MIPS_SR_COP_1_BIT	# Turn off the FPU.
	.set	noat
/*
 * Call the exception handler.
 */
	jal	_C_LABEL(trap)
	mtc0	t0, MIPS_COP_0_STATUS_REG
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# this should disable interrupts
	mtlo	t0
	mthi	t1
	lw	k0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)

	RESTORE_USER_REGS(UADDR)

	j	k0
	rfe
	.set	at
END(mips1_UserGenException)

/*----------------------------------------------------------------------------
 *
 * mips1_KernIntr --
 *
 *	Handle an interrupt from kernel mode.
 *	Interrupts use the standard kernel stack.
 *	switch_exit sets up a kernel stack after exit so interrupts won't fail.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
#define KINTR_REG_OFFSET	(STAND_FRAME_SIZE)
#define KINTR_SR_OFFSET		(STAND_FRAME_SIZE + KERN_REG_SIZE)
#define KINTR_MULT_LO_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 4)
#define KINTR_MULT_HI_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 8)
#define KINTR_GP_OFFSET		(STAND_FRAME_SIZE + KERN_REG_SIZE + 12)
#define	KINTR_FRAME_SIZE	(STAND_FRAME_SIZE + KERN_REG_SIZE + 16)

NNON_LEAF(mips1_KernIntr, KINTR_FRAME_SIZE, ra)
	.set	noat
	subu	sp, sp, KINTR_FRAME_SIZE	# allocate stack frame
	.mask	0x80000000, (STAND_RA_OFFSET - KINTR_FRAME_SIZE)
/*
 * Save the relevant kernel registers onto the stack.
 * We don't need to save s0 - s8 and sp because
 * the compiler does it for us.
 */
	sw	AT, KINTR_REG_OFFSET + 0(sp)
	sw	v0, KINTR_REG_OFFSET + 4(sp)
	sw	v1, KINTR_REG_OFFSET + 8(sp)
	sw	a0, KINTR_REG_OFFSET + 12(sp)
	mflo	v0
	mfhi	v1
	sw	a1, KINTR_REG_OFFSET + 16(sp)
	sw	a2, KINTR_REG_OFFSET + 20(sp)
	sw	a3, KINTR_REG_OFFSET + 24(sp)
	sw	t0, KINTR_REG_OFFSET + 28(sp)
	mfc0	a0, MIPS_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, KINTR_REG_OFFSET + 32(sp)
	sw	t2, KINTR_REG_OFFSET + 36(sp)
	sw	t3, KINTR_REG_OFFSET + 40(sp)
	sw	t4, KINTR_REG_OFFSET + 44(sp)
	mfc0	a1, MIPS_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, KINTR_REG_OFFSET + 48(sp)
	sw	t6, KINTR_REG_OFFSET + 52(sp)
	sw	t7, KINTR_REG_OFFSET + 56(sp)
	sw	t8, KINTR_REG_OFFSET + 60(sp)
	mfc0	a2, MIPS_COP_0_EXC_PC		# Third arg is the pc.
	sw	t9, KINTR_REG_OFFSET + 64(sp)
	sw	ra, KINTR_REG_OFFSET + 68(sp)
	sw	v0, KINTR_MULT_LO_OFFSET(sp)
	sw	v1, KINTR_MULT_HI_OFFSET(sp)
	sw	a0, KINTR_SR_OFFSET(sp)
	sw	gp, KINTR_GP_OFFSET(sp)
#ifdef __GP_SUPPORT__
	la	gp, _C_LABEL(_gp)		# switch to kernel GP
#endif
/*
 * Call the interrupt handler.
 */
	jal	_C_LABEL(interrupt)
	sw	a2, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the interrupt.
 */
	lw	a0, KINTR_SR_OFFSET(sp)
	lw	t0, KINTR_MULT_LO_OFFSET(sp)
	lw	t1, KINTR_MULT_HI_OFFSET(sp)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	lw	k0, STAND_RA_OFFSET(sp)
	lw	AT, KINTR_REG_OFFSET + 0(sp)
	lw	v0, KINTR_REG_OFFSET + 4(sp)

	RESTORE_KERN_REGISTERS(KINTR_REG_OFFSET)

	addu	sp, sp, KINTR_FRAME_SIZE
	j	k0				# Now return from the
	rfe					#  interrupt.
	.set	at
END(mips1_KernIntr)

/*----------------------------------------------------------------------------
 *
 * mips1_UserIntr --
 *
 *	Handle an interrupt from user mode.
 *	Note: we save minimal state in the u.u_pcb struct and use the standard
 *	kernel stack since there has to be a u page if we came from user mode.
 *	If there is a pending software interrupt, then save the remaining state
 *	and call softintr(). This is all because if we call switch() inside
 *	interrupt(), not all the user registers have been saved in u.u_pcb.
 *
 * Results:
 * 	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
NNON_LEAF(mips1_UserIntr, STAND_FRAME_SIZE, ra)
	.set	noat
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
/*
 * Save the relevant user registers into the u.u_pcb struct.
 * We don't need to save s0 - s8 because
 * the compiler does it for us.
 */
	sw	AT, UADDR+U_PCB_REGS+(AST * 4)
	sw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	sw	v1, UADDR+U_PCB_REGS+(V1 * 4)
	sw	a0, UADDR+U_PCB_REGS+(A0 * 4)
	mflo	v0
	mfhi	v1
	sw	a1, UADDR+U_PCB_REGS+(A1 * 4)
	sw	a2, UADDR+U_PCB_REGS+(A2 * 4)
	sw	a3, UADDR+U_PCB_REGS+(A3 * 4)
	sw	t0, UADDR+U_PCB_REGS+(T0 * 4)
	mfc0	a0, MIPS_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	sw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	sw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	sw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	mfc0	a1, MIPS_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a2, MIPS_COP_0_EXC_PC		# Third arg is the pc.
	sw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	sw	gp, UADDR+U_PCB_REGS+(GP * 4)
	sw	sp, UADDR+U_PCB_REGS+(SP * 4)
	sw	ra, UADDR+U_PCB_REGS+(RA * 4)
	li	sp, KERNELSTACK - STAND_FRAME_SIZE	# switch to kernel SP
	sw	v0, UADDR+U_PCB_REGS+(MULLO * 4)
	sw	v1, UADDR+U_PCB_REGS+(MULHI * 4)
	sw	a0, UADDR+U_PCB_REGS+(SR * 4)
	sw	a2, UADDR+U_PCB_REGS+(PC * 4)
#ifdef __GP_SUPPORT__
	la	gp, _C_LABEL(_gp)		# switch to kernel GP
#endif
	.set	at
	and	t0, a0, ~MIPS_SR_COP_1_BIT	# Turn off the FPU.
	.set	noat
	mtc0	t0, MIPS_COP_0_STATUS_REG
/*
 * Call the interrupt handler.
 */
	jal	_C_LABEL(interrupt)
	sw	a2, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the interrupt.
 */
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	v0, _C_LABEL(astpending)	# any pending interrupts?
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	bne	v0, zero, 1f			# dont restore, call softintr
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	lw	k0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	lw	v1, UADDR+U_PCB_REGS+(V1 * 4)
	lw	a0, UADDR+U_PCB_REGS+(A0 * 4)
	lw	a1, UADDR+U_PCB_REGS+(A1 * 4)
	lw	a2, UADDR+U_PCB_REGS+(A2 * 4)
	lw	a3, UADDR+U_PCB_REGS+(A3 * 4)
	mtlo	t0
	mthi	t1
	lw	t0, UADDR+U_PCB_REGS+(T0 * 4)
	lw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	lw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	lw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	lw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	lw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	lw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	lw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	lw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	lw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	lw	gp, UADDR+U_PCB_REGS+(GP * 4)
	lw	sp, UADDR+U_PCB_REGS+(SP * 4)
	lw	ra, UADDR+U_PCB_REGS+(RA * 4)
	j	k0				# Now return from the
	rfe					#  interrupt.

1:
/*
 * We have pending software interrupts; save remaining user state in u.u_pcb.
 */
	sw	s0, UADDR+U_PCB_REGS+(S0 * 4)
	sw	s1, UADDR+U_PCB_REGS+(S1 * 4)
	sw	s2, UADDR+U_PCB_REGS+(S2 * 4)
	sw	s3, UADDR+U_PCB_REGS+(S3 * 4)
	sw	s4, UADDR+U_PCB_REGS+(S4 * 4)
	sw	s5, UADDR+U_PCB_REGS+(S5 * 4)
	sw	s6, UADDR+U_PCB_REGS+(S6 * 4)
	sw	s7, UADDR+U_PCB_REGS+(S7 * 4)
	sw	s8, UADDR+U_PCB_REGS+(S8 * 4)
	li	t0, MIPS_HARD_INT_MASK | MIPS_SR_INT_IE
/*
 * Call the software interrupt handler.
 */
	jal	_C_LABEL(softintr)
	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable interrupts (spl0)
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# this should disable interrupts
	mtlo	t0
	mthi	t1
	lw	k0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)

	RESTORE_USER_REGS(UADDR)

	j	k0
	rfe
	.set	at
END(mips1_UserIntr)

/*
 *  Mark where code entreed from exception hander jumptable
 * ends, for stack traceback code.
 */

	.globl	_C_LABEL(mips1_exceptionentry_end)
_C_LABEL(mips1_exceptionentry_end):


/*----------------------------------------------------------------------------
 *
 *		XXX  START of r3000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */



#if 0
/*----------------------------------------------------------------------------
 *
 * mips1_TLBModException --
 *
 *	Handle a TLB modified exception.
 *	The BaddVAddr, Context, and EntryHi registers contain the failed
 *	virtual address.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
NLEAF(mips1_TLBModException)
	.set	noat
	tlbp					# find the TLB entry
	mfc0	k0, MIPS_COP_0_TLB_LOW		# get the physical address
	mfc0	k1, MIPS_COP_0_TLB_INDEX	# check to be sure its valid
	or	k0, k0, MIPS1_TLB_MOD_BIT	# update TLB
	blt	k1, zero, 4f			# not found!!!
	mtc0	k0, MIPS_COP_0_TLB_LOW
	li	k1, MIPS_KSEG0_START
	subu	k0, k0, k1
	srl	k0, k0, MIPS1_TLB_PHYS_PAGE_SHIFT
	la	k1, pmap_attributes
	addu	k0, k0, k1
	lbu	k1, 0(k0)			# fetch old value
	nop
	or	k1, k1, 1			# set modified bit
	sb	k1, 0(k0)			# save new value
	mfc0	k0, MIPS_COP_0_EXC_PC		# get return address
	nop
	j	k0
	rfe
4:
	break	0				# panic
	.set	at
END(mips1_TLBModException)
#endif

/*----------------------------------------------------------------------------
 *
 * mips1_TLBMissException --
 *
 *	Handle a TLB miss exception from kernel mode.
 *	The BaddVAddr, Context, and EntryHi registers contain the failed
 *	virtual address.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
NLEAF(mips1_TLBMissException)
	.set	noat
	mfc0	k0, MIPS_COP_0_BAD_VADDR	# get the fault address
	li	k1, VM_MIN_KERNEL_ADDRESS	# compute index
	subu	k0, k0, k1
	lw	k1, _C_LABEL(Sysmapsize)	# index within range?
	srl	k0, k0, PGSHIFT
	sltu	k1, k0, k1
	beq	k1, zero, 1f			# No. check for valid stack
	nop
	lw	k1, _C_LABEL(Sysmap)
	sll	k0, k0, 2			# compute offset from index
	addu	k1, k1, k0
	lw	k0, 0(k1)			# get PTE entry
	mfc0	k1, MIPS_COP_0_EXC_PC		# get return address
	mtc0	k0, MIPS_COP_0_TLB_LOW		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, _C_LABEL(mips1_KernGenException) # PTE invalid
	nop
	tlbwr					# update TLB
	j	k1
	rfe

1:
	subu	k0, sp, UADDR + 0x200		# check to see if we have a
	sltiu	k0, UPAGES*NBPG - 0x200		#  valid kernel stack
	bne	k0, zero, _C_LABEL(mips1_KernGenException) # Go panic
	nop

	la	a0, start - START_FRAME - 8	# set sp to a valid place
	sw	sp, 24(a0)
	move	sp, a0
	la	a0, 1f
	mfc0	a2, MIPS_COP_0_STATUS_REG
	mfc0	a3, MIPS_COP_0_CAUSE_REG
	mfc0	a1, MIPS_COP_0_EXC_PC
	sw	a2, 16(sp)
	sw	a3, 20(sp)
	sw	sp, 24(sp)
	move	a2, ra
	jal	_C_LABEL(printf)
	mfc0	a3, MIPS_COP_0_BAD_VADDR
	.data
1:
	.asciiz	"ktlbmiss: PC %x RA %x ADR %x\nSR %x CR %x SP %x\n"
	.text

	la	sp, start - START_FRAME		# set sp to a valid place
	PANIC("kernel stack overflow")
	.set	at
END(mips1_TLBMissException)


/*--------------------------------------------------------------------------
 *
 * mips1_TLBWriteIndexed --
 *
 *	Write the given entry into the TLB at the given index.
 *
 *	mips1_TLBWriteIndexed(index, highEntry, lowEntry)
 *		int index;
 *		int highEntry;
 *		int lowEntry;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TLB entry set.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBWriteIndexed)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Save the current PID.

	sll	a0, a0, MIPS1_TLB_INDEX_SHIFT
	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index.
	mtc0	a1, MIPS_COP_0_TLB_HI		# Set up entry high.
	mtc0	a2, MIPS_COP_0_TLB_LOW		# Set up entry low.
	nop
	tlbwi					# Write the TLB

	mtc0	t0, MIPS_COP_0_TLB_HI		# Restore the PID.
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBWriteIndexed)

#if 0
/*--------------------------------------------------------------------------
 *
 * mips1_TLBWriteRandom --
 *
 *	Write the given entry into the TLB at a random location.
 *
 *	mips1_TLBWriteRandom(highEntry, lowEntry)
 *		unsigned highEntry;
 *		unsigned lowEntry;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TLB entry set.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBWriteRandom)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	v0, MIPS_COP_0_TLB_HI		# Save the current PID.
	nop

	mtc0	a0, MIPS_COP_0_TLB_HI		# Set up entry high.
	mtc0	a1, MIPS_COP_0_TLB_LOW		# Set up entry low.
	nop
	tlbwr					# Write the TLB

	mtc0	v0, MIPS_COP_0_TLB_HI		# Restore the PID.
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBWriteRandom)
#endif

/*--------------------------------------------------------------------------
 *
 * mips1_SetPID --
 *
 *	Write the given pid into the TLB pid reg.
 *
 *	mips1_SetPID(pid)
 *		int pid;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PID set in the entry hi register.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_SetPID)
	sll	a0, a0, MIPS_TLB_PID_SHIFT	# put PID in right spot
	mtc0	a0, MIPS_COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(mips1_SetPID)

/*--------------------------------------------------------------------------
 *
 * mips1_TLBFlush --
 *
 *	Flush the "random" entries from the TLB.
 *
 *	mips1_TLBFlush()
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBFlush)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Save the PID
	li	t1, MIPS_KSEG0_START		# invalid address
	mtc0	t1, MIPS_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MIPS_COP_0_TLB_LOW	# Zero out low entry.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
	li	t1, MIPS1_TLB_FIRST_RAND_ENTRY << MIPS1_TLB_INDEX_SHIFT
	li	t2, MIPS1_TLB_NUM_TLB_ENTRIES << MIPS1_TLB_INDEX_SHIFT
1:
	mtc0	t1, MIPS_COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, 1 << MIPS1_TLB_INDEX_SHIFT	# Increment index.
	bne	t1, t2, 1b
	tlbwi					# Write the TLB entry.

	mtc0	t0, MIPS_COP_0_TLB_HI		# Restore the PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBFlush)

#if 0
/*--------------------------------------------------------------------------
 *
 * mips1_TLBFlushPID --
 *
 *	Flush all entries with the given PID from the TLB.
 *
 *	mips1_TLBFlushPID(pid)
 *		int pid;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All entries corresponding to this PID are flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBFlushPID)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Save the current PID
	sll	a0, a0, MIPS_TLB_PID_SHIFT	# Align the pid to flush.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
	li	t1, MIPS1_FIRST_RAND_ENTRY << MIPS1_TLB_INDEX_SHIFT
	li	t2, MIPS1_NUM_TLB_ENTRIES << MIPS1_TLB_INDEX_SHIFT
	mtc0	t1, MIPS_COP_0_TLB_INDEX	# Set the index register
1:
	addu	t1, t1, 1 << MIPS1_TLB_INDEX_SHIFT	# Increment index.
	tlbr					# Read from the TLB
	mfc0	t4, MIPS_COP_0_TLB_HI		# Fetch the hi register.
	nop
	and	t4, t4, MIPS1_TLB_PID		# compare PIDs
	bne	t4, a0, 2f
	li	v0, MIPS_KSEG0_START	# invalid address
	mtc0	v0, MIPS_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MIPS_COP_0_TLB_LOW	# Zero out low entry.
	nop
	tlbwi					# Write the entry.
2:
	bne	t1, t2, 1b
	mtc0	t1, MIPS_COP_0_TLB_INDEX	# Set the index register

	mtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBFlushPID)
#endif

/*--------------------------------------------------------------------------
 *
 * mips1_TLBFlushAddr --
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 *	mips1_TLBFlushAddr(highreg)
 *		unsigned highreg;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The process's page is flushed from the TLB.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBFlushAddr)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID
	nop

	mtc0	a0, MIPS_COP_0_TLB_HI		# look for addr & PID
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MIPS_COP_0_TLB_INDEX	# See what we got
	li	t1, MIPS_KSEG0_START	# Load invalid entry.
	bltz	v0, 1f				# index < 0 => !found
	mtc0	t1, MIPS_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MIPS_COP_0_TLB_LOW	# Zero out low entry.
	nop
	tlbwi
1:
	mtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * mips1_TLBUpdate --
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 *	mips1_TLBUpdate(highreg, lowreg)
 *		unsigned highreg, lowreg;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBUpdate)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Save current PID
	nop					# 2 cycles before intr disabled
	mtc0	a0, MIPS_COP_0_TLB_HI		# init high reg.
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MIPS_COP_0_TLB_INDEX	# See what we got
	mtc0	a1, MIPS_COP_0_TLB_LOW		# init low reg.
	bltz	v0, 1f				# index < 0 => !found
	sra	v0, v0, MIPS1_TLB_INDEX_SHIFT	# convert index to regular num
	b	2f
	tlbwi					# update slot found
1:
	mtc0	a0, MIPS_COP_0_TLB_HI		# init high reg.
	nop
	tlbwr					# enter into a random slot
2:
	mtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBUpdate)

/*--------------------------------------------------------------------------
 *
 * mips1_TLBFind --
 *
 *	Search the TLB for the given entry.
 *
 *	mips1_TLBFind(hi)
 *		unsigned hi;
 *
 * Results:
 *	Returns a value >= 0 if the entry was found (the index).
 *	Returns a value < 0 if the entry was not found.
 *
 * Side effects:
 *	tlbhi and tlblo will contain the TLB entry found.
 *
 *--------------------------------------------------------------------------
 */
	.comm	tlbhi, 4
	.comm	tlblo, 4
LEAF(mips1_TLBFind)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID
	nop
	mtc0	a0, MIPS_COP_0_TLB_HI		# Set up entry high.
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MIPS_COP_0_TLB_INDEX	# See what we got
	nop
	bltz	v0, 1f				# not found
	nop
	tlbr					# read TLB
	mfc0	t1, MIPS_COP_0_TLB_HI		# See what we got
	mfc0	t2, MIPS_COP_0_TLB_LOW		# See what we got
	sw	t1, tlbhi
	sw	t2, tlblo
	srl	v0, v0, MIPS1_TLB_INDEX_SHIFT	# convert index to regular num
1:
	mtc0	t0, MIPS_COP_0_TLB_HI		# Restore current PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBFind)

/*--------------------------------------------------------------------------
 *
 * mips1_TLBRead --
 *
 *	Read the TLB entry.
 *
 *	mips1_TLBRead(entry)
 *		unsigned entry;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tlbhi and tlblo will contain the TLB entry found.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBRead)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID

	sll	a0, a0, MIPS1_TLB_INDEX_SHIFT
	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr					# Read from the TLB
	mfc0	t3, MIPS_COP_0_TLB_HI		# fetch the hi entry
	mfc0	t4, MIPS_COP_0_TLB_LOW		# fetch the low entry
	sw	t3, tlbhi
	sw	t4, tlblo

	mtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips1_TLBRead)

/*--------------------------------------------------------------------------
 *
 * mips1_TLBGetPID --
 *
 *	mips1_TLBGetPID()
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips1_TLBGetPID)
	mfc0	v0, MIPS_COP_0_TLB_HI		# get PID
	nop
	and	v0, v0, MIPS1_TLB_PID		# mask off PID
	j	ra
	srl	v0, v0, MIPS_TLB_PID_SHIFT	# put PID in right spot
END(mips1_TLBGetPID)


/*----------------------------------------------------------------------------
 *
 *	R3000 cache sizing and flushing code.
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 * mips1_ConfigCache --
 *
 *	Size the caches.
 *	NOTE: should only be called from mach_init().
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The size of the data cache is stored into machDataCacheSize and the
 *	size of instruction cache is stored into machInstCacheSize.
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(mips1_ConfigCache, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	sw	ra, STAND_RA_OFFSET(sp)		# Save return address.
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts.
	la	v0, 1f
	or	v0, MIPS_KSEG1_START	# Run uncached.
	j	v0
	nop
1:
/*
 * This works because jal doesn't change pc[31..28] and the
 * linker still thinks SizeCache is in the cached region so it computes
 * the correct address without complaining.
 */
	jal	_C_LABEL(mips1_SizeCache)	# Get the size of the d-cache.
	nop
	sw	v0, _C_LABEL(machDataCacheSize)
	nop					# Make sure sw out of pipe
	nop
	nop
	nop
	li	v0, MIPS_SR_SWAP_CACHES		# Swap caches
	mtc0	v0, MIPS_COP_0_STATUS_REG
	nop					# Insure caches stable
	nop
	nop
	nop
	jal	_C_LABEL(mips1_SizeCache)	# Get the size of the i-cache.
	nop
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Swap back caches and enable.
	nop
	nop
	nop
	nop
	sw	v0, _C_LABEL(machInstCacheSize)
	la	t0, 1f
	j	t0				# Back to cached mode
	nop
1:
	lw	ra, STAND_RA_OFFSET(sp)		# Restore return addr
	addu	sp, sp, STAND_FRAME_SIZE	# Restore sp.
	j	ra
	nop
END(mips1_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * mips1_SizeCache --
 *
 *	Get the size of the cache.
 *
 * Results:
 *	The size of the cache.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips1_SizeCache)
	mfc0	t0, MIPS_COP_0_STATUS_REG	# Save the current status reg.
	nop
	or	v0, t0, MIPS_SR_ISOL_CACHES	# Isolate the caches.
	nop					# Make sure no stores in pipe
	mtc0	v0, MIPS_COP_0_STATUS_REG
	nop					# Make sure isolated
	nop
	nop
/*
 * Clear cache size boundaries.
 */
	li	v0, MIPS_MIN_CACHE_SIZE
	li	v1, MIPS_KSEG0_START
	li	t2, MIPS_MAX_CACHE_SIZE
1:
	addu	t1, v0, v1			# Compute address to clear
	sw	zero, 0(t1)			# Clear cache memory
	bne	v0, t2, 1b
	sll	v0, v0, 1

	li	v0, -1
	sw	v0, 0(v1)			# Store marker in cache
	li	v0, MIPS_MIN_CACHE_SIZE
2:
	addu	t1, v0, v1			# Compute address
	lw	t3, 0(t1)			# Look for marker
	nop
	bne	t3, zero, 3f			# Found marker.
	nop
	bne	v0, t2, 2b			# keep looking
	sll	v0, v0, 1			# cache size * 2

	move	v0, zero			# must be no cache
3:
	mtc0	t0, MIPS_COP_0_STATUS_REG
	nop					# Make sure unisolated
	nop
	nop
	nop
	j	ra
	nop
END(mips1_SizeCache)

/*----------------------------------------------------------------------------
 *
 * mips1_FlushCache --
 *
 *	Flush the caches.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the caches is flushed.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips1_FlushCache)
	lw	t1, _C_LABEL(machInstCacheSize)	# Must load before isolating
	lw	t2, _C_LABEL(machDataCacheSize)	# Must load before isolating
	mfc0	t3, MIPS_COP_0_STATUS_REG 	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts.
	la	v0, 1f
	or	v0, MIPS_KSEG1_START	# Run uncached.
	j	v0
	nop
/*
 * Flush the instruction cache.
 */
1:
	li	v0, MIPS_SR_ISOL_CACHES | MIPS_SR_SWAP_CACHES
	mtc0	v0, MIPS_COP_0_STATUS_REG	# Isolate and swap caches.
	li	t0, MIPS_KSEG1_START
	subu	t0, t0, t1
	li	t1, MIPS_KSEG1_START
	la	v0, 1f				# Run cached
	j	v0
	nop
1:
	addu	t0, t0, 4
	bne	t0, t1, 1b
	sb	zero, -4(t0)

	la	v0, 1f
	or	v0, MIPS_KSEG1_START
	j	v0				# Run uncached
	nop
/*
 * Flush the data cache.
 */
1:
	li	v0, MIPS_SR_ISOL_CACHES
	mtc0	v0, MIPS_COP_0_STATUS_REG	# Isolate and swap back caches
	li	t0, MIPS_KSEG1_START
	subu	t0, t0, t2
	la	v0, 1f
	j	v0				# Back to cached mode
	nop
1:
	addu	t0, t0, 4
	bne	t0, t1, 1b
	sb	zero, -4(t0)

	nop					# Insure isolated stores
	nop					#   out of pipe.
	nop
	nop
	mtc0	t3, MIPS_COP_0_STATUS_REG	# Restore status reg.
	nop					# Insure cache unisolated.
	nop
	nop
	nop
	j	ra
	nop
END(mips1_FlushCache)

/*----------------------------------------------------------------------------
 *
 * mips1_FlushICache --
 *
 *	void mips1_FlushICache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush instruction cache for range of addr to addr + len - 1.
 *	The address can be any valid address so long as no TLB misses occur.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is flushed.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips1_FlushICache)
	mfc0	t0, MIPS_COP_0_STATUS_REG	# Save SR
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts.

	la	v1, 1f
	or	v1, MIPS_KSEG1_START	# Run uncached.
	j	v1
	nop
1:
	bc0f	1b				# make sure stores are complete
	li	v1, MIPS_SR_ISOL_CACHES | MIPS_SR_SWAP_CACHES
	mtc0	v1, MIPS_COP_0_STATUS_REG
	nop
	addu	a1, a1, a0			# compute ending address
1:
	addu	a0, a0, 4
	bne	a0, a1, 1b
	sb	zero, -4(a0)

	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable interrupts
	j	ra				# return and run cached
	nop
END(mips1_FlushICache)

/*----------------------------------------------------------------------------
 *
 * mips1_FlushDCache --
 *
 *	void mips1_FlushDCache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush data cache for range of addr to addr + len - 1.
 *	The address can be any valid address so long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses)
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is flushed.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips1_FlushDCache)
	mfc0	t0, MIPS_COP_0_STATUS_REG	# Save SR
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts.
	nop
1:
	bc0f	1b				# make sure stores are complete
# BUG: should drain write buffer.
# The insn above does not work on some all DEC machines, or all variants
# of the mips architecture.
	li	v1, MIPS_SR_ISOL_CACHES
	mtc0	v1, MIPS_COP_0_STATUS_REG
	nop
	addu	t1, a1, a0			# compute ending address
1:
	sb	zero, 0(a0)
	sb	zero, 4(a0)
	sb	zero, 8(a0)
	sb	zero, 12(a0)
	sb	zero, 16(a0)
	sb	zero, 20(a0)
	sb	zero, 24(a0)
	addu	a0, 32
	bltu	a0, t1, 1b
	sb	zero, -4(a0)

	nop					# drain pipeline
	nop
	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable interrupts
	nop
	j	ra				# return and run cached
	nop
END(mips1_FlushDCache)

/*----------------------------------------------------------------------------
 *
 *		XXX  END of r3000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */

