
/*
 *============================================================================
 *
 *  Mips-III  ISA support, part 1: locore exception vectors.
 *  The following code is copied to the vector locations to which
 *  the CPU jumps in response to an exception or a TLB miss.
 *
 *============================================================================


/*
 *----------------------------------------------------------------------------
 *
 * mips3_TLBMiss --
 * MachTLBMiss --
 *
 *	Vector code for the TLB-miss exception vector 0x80000180
 *	on an r4000.
 *
 * This code is copied to the TLB exception vector address to
 * handle TLB translation misses.
 * NOTE: This code must be relocatable and max 32 instructions!!!
 * Don't check for invalid pte's here. We load them as well and
 * let the processor trap to load the correct value after service.
 *
 *----------------------------------------------------------------------------
 */
	.globl	_C_LABEL(mips3_TLBMiss)
_C_LABEL(mips3_TLBMiss):
	.globl	_C_LABEL(MachTLBMiss)
_C_LABEL(MachTLBMiss):
	.set	noat
	dmfc0	k0, MIPS_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, UADDR+U_PCB_SEGTAB		# get the current segment table
	bltz	k0, 1f				# kernel address space ->
	srl	k0, k0, SEGSHIFT - 2		# compute segment table index
	andi	k0, k0, 0x7fc			# PMAP_SEGTABSIZ-1
	addu	k1, k1, k0
	dmfc0	k0, MIPS_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, 0(k1)			# get pointer to segment map
	srl	k0, k0, PGSHIFT - 2		# compute segment map index
	andi	k0, k0, ((NPTEPG/2) - 1) << 3
	beq	k1, zero, 2f			# invalid segment map
	addu	k1, k1, k0			# index into segment map
	lw	k0, 0(k1)			# get page PTE
	lw	k1, 4(k1)
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO0
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, MIPS_COP_0_TLB_LO1
	nop
	tlbwr					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
1:
	j	mips3_TLBMissException
	nop
2:
	j	mips3_SlowFault
	nop

	.globl	_C_LABEL(MachTLBMissEnd)
_C_LABEL(MachTLBMissEnd):
	.globl	_C_LABEL(mips3_TLBMissEnd)
_C_LABEL(mips3_TLBMissEnd):
	.set	at

/*
 *----------------------------------------------------------------------------
 *
 * mips3_execption --
 *
 *	Vector code for the general exception vector 0x80000080
 *	on an r4000 or r4400.
 *
 * This code is copied to the general exception vector address to
 * handle all execptions except RESET and TLBMiss.
 * NOTE: This code must be relocatable!!!
 *----------------------------------------------------------------------------
 */
	.globl	mips3_exception
_C_LABEL(mips3_exception):
/*
 * Find out what mode we came from and jump to the proper handler.
 */
	.set	noat
	mfc0	k0, MIPS_COP_0_STATUS_REG	# Get the status register
	mfc0	k1, MIPS_COP_0_CAUSE_REG	# Get the cause register value.
	and	k0, k0, MIPS_4K_SR_KSU_USER	# test for user mode
						# sneaky but the bits are
						# with us........
	sll	k0, k0, 3			# shift user bit for cause index
	and	k1, k1, MIPS_4K_CR_EXC_CODE	# Mask out the cause bits.
	or	k1, k1, k0			# change index to user table
1:
	la	k0, mips3_ExceptionTable	# get base of the jump table
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
	.globl	mips3_exceptionEnd
_C_LABEL(mips3_exceptionEnd):


/*----------------------------------------------------------------------------
 *
 * mips3_SlowFault --
 *
 * Alternate entry point into the mips_r2000_UserGenExceptionor or
 * or mips_r2000_user_Kern_exception, when the ULTB miss handler couldn't
 * find a TLB entry.
 *
 * Find out what mode we came from and call the appropriate handler.
 *
 *----------------------------------------------------------------------------
 */

/*
 * We couldn't find a TLB entry.
 * Find out what mode we came from and call the appropriate handler.
 */
mips3_SlowFault:
	.set	noat
	mfc0	k0, MIPS_COP_0_STATUS_REG
	nop
	and	k0, k0, MIPS_SR_KSU_USER
	bne	k0, zero, mips3_UserGenException
	nop
	.set	at
/*
 * Fall though ...
 */

/*----------------------------------------------------------------------------
 *
 * mips3_KernGenException --
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

/*
 * Similar definition for interrupt-exception frames.
 */
#define KINTR_REG_OFFSET	(STAND_FRAME_SIZE)
#define KINTR_SR_OFFSET		(STAND_FRAME_SIZE + KERN_REG_SIZE)
#define KINTR_MULT_LO_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 4)
#define KINTR_MULT_HI_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 8)
#define KINTR_MULT_GP_OFFSET	(STAND_FRAME_SIZE + KERN_REG_SIZE + 12)
#define	KINTR_FRAME_SIZE	(STAND_FRAME_SIZE + KERN_REG_SIZE + 16)


NNON_LEAF(mips3_KernGenException, KERN_EXC_FRAME_SIZE, ra)
	.set	noat
#ifdef DEBUG
	la	k0, mdbpcb			# save registers for mdb
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

	mtc0	zero,MIPS_COP_0_STATUS_REG	# Set kernel no error level
/*
 * Call the exception handler.
 */
	jal	trap
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the exception.
 * v0 contains the return address.
 */
	mtc0	zero,MIPS_COP_0_STATUS_REG	# Make shure int disabled
	lw	a0, KERN_SR_OFFSET(sp)
	lw	t0, KERN_MULT_LO_OFFSET(sp)
	lw	t1, KERN_MULT_HI_OFFSET(sp)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	dmtc0	v0, MIPS_COP_0_EXC_PC		# set return address

	lw	AT, KERN_REG_OFFSET + 0(sp)
	lw	v0, KERN_REG_OFFSET + 4(sp)

	RESTORE_KERN_REGISTERS(KINTR_REG_OFFSET)

	addu	sp, sp, KERN_EXC_FRAME_SIZE
	eret					#  exception.
	.set	at
END(mips3_KernGenException)

/*----------------------------------------------------------------------------
 *
 * mips3_UserGenException --
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
NNON_LEAF(mips3_UserGenException, STAND_FRAME_SIZE, ra)
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
	la	gp, _gp				# switch to kernel GP
	sw	a3, UADDR+U_PCB_REGS+(PC * 4)
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
	.set	at
# Turn off fpu and enter kernel mode
	and	t0, a0, ~(MIPS_SR_COP_1_BIT | MIPS_SR_EXL | MIPS_SR_KSU_MASK | MIPS_SR_INT_IE)
	.set	noat
/*
 * Call the exception handler.
 */
	jal	trap
	mtc0	t0, MIPS_COP_0_STATUS_REG
/*
 * Restore user registers and return.
 * First disable interrupts and set exeption level.
 */
	mtc0	zero, MIPS_COP_0_STATUS_REG	# disable int
	nop
	nop
	nop
	li	v0, MIPS_SR_EXL
	mtc0	v0, MIPS_COP_0_STATUS_REG	# set exeption level

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# still exeption level
	mtlo	t0
	mthi	t1
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, MIPS_COP_0_EXC_PC		# set return address

	RESTORE_USER_REGS(UADDR)	/* XXX FIXME */

	eret
	.set	at
END(mips3_UserGenException)

/*----------------------------------------------------------------------------
 *
 * mips3_KernIntr --
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
NNON_LEAF(mips3_KernIntr, KINTR_FRAME_SIZE, ra)
	.set	noat
	subu	sp, sp, KINTR_FRAME_SIZE	# allocate stack frame
	.mask	0x80000000, (STAND_RA_OFFSET - KINTR_FRAME_SIZE)
/*
 * Save the relevant kernel registers onto the stack.
 * We don't need to save s0 - s8, sp and gp because
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

	mtc0	zero, MIPS_COP_0_STATUS_REG	# Reset exl, trap possible.
/*
 * Call the interrupt handler.
 */
	jal	interrupt
	sw	a2, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the interrupt.
 */
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupt
	lw	a0, KINTR_SR_OFFSET(sp)
	lw	t0, KINTR_MULT_LO_OFFSET(sp)
	lw	t1, KINTR_MULT_HI_OFFSET(sp)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	lw	a0, STAND_RA_OFFSET(sp)
	lw	AT, KINTR_REG_OFFSET + 0(sp)
	lw	v0, KINTR_REG_OFFSET + 4(sp)
	dmtc0	a0, MIPS_COP_0_EXC_PC		# set return address

	RESTORE_KERN_REGISTERS(KINTR_REG_OFFSET)

	addu	sp, sp, KINTR_FRAME_SIZE
	eret					#  interrupt.
	.set	at
END(mips3_KernIntr)

/*----------------------------------------------------------------------------
 *
 * mips3_UserIntr --
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
NNON_LEAF(mips3_UserIntr, STAND_FRAME_SIZE, ra)
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
	la	gp, _gp				# switch to kernel GP
# Turn off fpu and enter kernel mode
	.set	at
	and	t0, a0, ~(MIPS_SR_COP_1_BIT | MIPS_SR_EXL | MIPS_SR_INT_IE | MIPS_SR_KSU_MASK)
	.set	noat
	mtc0	t0, MIPS_COP_0_STATUS_REG
/*
 * Call the interrupt handler.
 */
	jal	interrupt
	sw	a2, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the interrupt.
 */
	mtc0	zero, MIPS_COP_0_STATUS_REG
	nop
	nop
	nop
	li	v0, MIPS_SR_EXL
	mtc0	v0, MIPS_COP_0_STATUS_REG	# set exeption level bit.

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	v0, astpending			# any pending interrupts?
	mtc0	a0, MIPS_COP_0_STATUS_REG	# Restore the SR, disable intrs
	bne	v0, zero, 1f			# dont restore, call softintr
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, MIPS_COP_0_EXC_PC		# set return address
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
	eret					#  interrupt.

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
	jal	softintr
	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable interrupts (spl0)
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	mtc0	zero, MIPS_COP_0_STATUS_REG
	nop
	nop
	nop
	li	v0, MIPS_SR_EXL
	mtc0	v0, MIPS_COP_0_STATUS_REG	# set exeption level bit.

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MIPS_COP_0_STATUS_REG	# this should disable interrupts
	mtlo	t0
	mthi	t1
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, MIPS_COP_0_EXC_PC		# set return address

	/*XXX*/
	RESTORE_USER_REGS(UADDR)	/* XXX FIXME */

	eret
	.set	at
END(mips3_UserIntr)


/*----------------------------------------------------------------------------
 *
 *		XXX  START of r4000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 *	R4000 TLB exception handlers
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 * mips3_TLBMInvalidException --
 *
 *	Handle a TLB invalid exception from kernel mode in kernel space.
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
NLEAF(mips3_TLBInvalidException)
	.set	noat
	dmfc0	k0, MIPS_COP_0_BAD_VADDR	# get the fault address
	li	k1, VM_MIN_KERNEL_ADDRESS	# compute index
	subu	k0, k0, k1
	lw	k1, Sysmapsize			# index within range?
	srl	k0, k0, PGSHIFT
	sltu	k1, k0, k1
	beq	k1, zero, sys_stk_chk		# No. check for valid stack
	lw	k1, Sysmap

	sll	k0, k0, 2			# compute offset from index
	tlbp					# Probe the invalid entry
	addu	k1, k1, k0
	and	k0, k0, 4			# check even/odd page
	bne	k0, zero, KernTLBIOdd
	nop

	mfc0	k0, MIPS_COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO0		# load PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, mips3_KernGenException	# PTE invalid
	lw	k0, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO1		# load PTE entry
	nop
	tlbwi					# write TLB
	nop
	nop
	nop
	nop
	nop
	eret

KernTLBIOdd:
	mfc0	k0, MIPS_COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of wired bit
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO1		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, mips3_KernGenException	# PTE invalid
	lw	k0, -4(k1)			# get even PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO0		# save PTE entry
	nop
	tlbwi					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
END(mips3_TLBInvalidException)

/*----------------------------------------------------------------------------
 *
 * mips3_TLBMissException --
 *
 *	Handle a TLB miss exception from kernel mode in kernel space.
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
NLEAF(mips3_TLBMissException)
	.set	noat
	dmfc0	k0, MIPS_COP_0_BAD_VADDR	# get the fault address
	li	k1, VM_MIN_KERNEL_ADDRESS	# compute index
	subu	k0, k0, k1
	lw	k1, Sysmapsize			# index within range?
	srl	k0, k0, PGSHIFT
	sltu	k1, k0, k1
	beq	k1, zero, sys_stk_chk		# No. check for valid stack
	lw	k1, Sysmap
	srl	k0, k0, 1
	sll	k0, k0, 3			# compute offset from index
	addu	k1, k1, k0
	lw	k0, 0(k1)			# get PTE entry
	lw	k1, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, MIPS_COP_0_TLB_LO0		# load PTE entry
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, MIPS_COP_0_TLB_LO1		# load PTE entry
	nop
	tlbwr					# write TLB
	nop
	nop
	nop
	nop
	nop
	eret

sys_stk_chk:
	subu	k0, sp, UADDR + 0x200		# check to see if we have a
	sltiu	k0, UPAGES*NBPG - 0x200		#  valid kernel stack
	bne	k0, zero, mips3_KernGenException	# Go panic
	nop

	la	a0, start - START_FRAME - 8	# set sp to a valid place
	sw	sp, 24(a0)
	move	sp, a0
	la	a0, 1f
	mfc0	a2, MIPS_COP_0_STATUS_REG
	mfc0	a3, MIPS_COP_0_CAUSE_REG
	dmfc0	a1, MIPS_COP_0_EXC_PC
	sw	a2, 16(sp)
	sw	a3, 20(sp)
	move	a2, ra
	jal	printf
	dmfc0	a3, MIPS_COP_0_BAD_VADDR
	.data
1:
	.asciiz	"ktlbmiss: PC %x RA %x ADR %x\nSR %x CR %x SP %x\n"
	.text

	la	sp, start - START_FRAME		# set sp to a valid place
	PANIC("kernel stack overflow")
	.set	at
END(mips3_TLBMissException)

/*
 *  Mark where code entreed from exception hander jumptable
 * ends, for stack traceback code.
 */

	.globl	_C_LABEL(mips3_exceptionentry_end)
_C_LABEL(mips3_exceptionentry_end):


/*--------------------------------------------------------------------------
 *
 * mips3_TLBWriteIndexedVPS --
 *
 *	Write the given entry into the TLB at the given index.
 *	Pass full r4000 tlb info icnluding variable page size mask.
 *
 *	mips3_TLBWriteIndexed(index, tlb)
 *		unsigned index;
 *		tlb *tlb;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TLB entry set.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBWriteIndexedVPS)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	nop
	lw	a2, 8(a1)
	lw	a3, 12(a1)
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Save the current PID.

	dmtc0	a2, MIPS_COP_0_TLB_LO0		# Set up entry low0.
	dmtc0	a3, MIPS_COP_0_TLB_LO1		# Set up entry low1.
	lw	a2, 0(a1)
	lw	a3, 4(a1)
	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index.
	dmtc0	a2, MIPS_COP_0_TLB_PG_MASK	# Set up entry mask.
	dmtc0	a3, MIPS_COP_0_TLB_HI		# Set up entry high.
	nop
	tlbwi					# Write the TLB
	nop
	nop
	nop					# Delay for effect
	nop

	dmtc0	t0, MIPS_COP_0_TLB_HI		# Restore the PID.
	nop
	dmtc0	zero, MIPS_COP_0_TLB_PG_MASK	# Default mask value.
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips3_TLBWriteIndexedVPS)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBWriteIndexed --
 *
 *	Write the given entry into the TLB at the given index.
 *
 *	mips3_TLBWriteIndexed(index, highentry, lowentry0, lowentry1)
 *		unsigned index;
 *		int highEntry;
 *		int lowEntry0;
 *		int lowEntry1;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TLB entry set.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBWriteIndexed)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	nop
	lw	a2, 8(a1)
	lw	a3, 12(a1)
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Save the current PID.

	dmtc0	a2, MIPS_COP_0_TLB_LO0		# Set up entry low0.
	dmtc0	a3, MIPS_COP_0_TLB_LO1		# Set up entry low1.
	lw	a2, 0(a1)
	lw	a3, 4(a1)
	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index.
	dmtc0	a2, MIPS_COP_0_TLB_PG_MASK	# Set up entry mask.
	dmtc0	a3, MIPS_COP_0_TLB_HI		# Set up entry high.
	nop
	tlbwi					# Write the TLB
	nop
	nop
	nop					# Delay for effect
	nop

	dmtc0	t0, MIPS_COP_0_TLB_HI		# Restore the PID.
	nop
	dmtc0	zero, MIPS_COP_0_TLB_PG_MASK	# Default mask value.
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips3_TLBWriteIndexed)

/*--------------------------------------------------------------------------
 *
 * mips3_SetPID --
 *
 *	Write the given pid into the TLB pid reg.
 *
 *	mips3_SetPID(pid)
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
LEAF(mips3_SetPID)
	dmtc0	a0, MIPS_COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(mips3_SetPID)

/*--------------------------------------------------------------------------
 *
 * mips3_SetWIRED --
 *
 *	Write the given value into the TLB wired reg.
 *
 *	mips3_SetPID(wired)
 *		int wired;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WIRED set in the wired register.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_SetWIRED)
	mtc0	a0, MIPS_COP_0_TLB_WIRED
	j	ra
	nop
END(mips3_SetWIRED)

/*--------------------------------------------------------------------------
 *
 * mips3_GetWIRED --
 *
 *	Get the value from the TLB wired reg.
 *
 *	mips3_GetWIRED(void)
 *
 * Results:
 *	Value of wired reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_GetWIRED)
	mfc0	v0, MIPS_COP_0_TLB_WIRED
	j	ra
	nop
END(mips3_GetWIRED)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBFlush --
 *
 *	Flush the "random" entries from the TLB.
 *	Uses "wired" register to determine what register to start with.
 *
 *	mips3_TLBFlush()
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBFlush)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t1, MIPS_COP_0_TLB_WIRED
	li	t2, MIPS3_TLB_NUM_TLB_ENTRIES
	li	v0, MIPS_KSEG0_START	# invalid address
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Save the PID

	dmtc0	v0, MIPS_COP_0_TLB_HI		# Mark entry high as invalid
	dmtc0	zero, MIPS_COP_0_TLB_LO0	# Zero out low entry0.
	dmtc0	zero, MIPS_COP_0_TLB_LO1	# Zero out low entry1.
	mtc0	zero, MIPS_COP_0_TLB_PG_MASK	# Zero out mask entry.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
1:
	mtc0	t1, MIPS_COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, 1			# Increment index.
	tlbwi					# Write the TLB entry.
	nop
	nop
	bne	t1, t2, 1b
	nop

	dmtc0	t0, MIPS_COP_0_TLB_HI		# Restore the PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips3_TLBFlush)


/*--------------------------------------------------------------------------
 *
 * mips3_TLBFlushAddr --
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 *	mips3_TLBFlushAddr(TLBhi)
 *		unsigned TLBhi;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The process's page is flushed from the TLB.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBFlushAddr)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	nop
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0			# Make shure valid hi value.
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID
	dmtc0	a0, MIPS_COP_0_TLB_HI		# look for addr & PID
	nop
	nop
	nop
	tlbp					# Probe for the entry.
	nop
	nop					# Delay for effect
	nop
	mfc0	v0, MIPS_COP_0_TLB_INDEX	# See what we got
	li	t1, MIPS_KSEG0_START	# Load invalid entry.
	bltz	v0, 1f				# index < 0 => !found
	nop
	dmtc0	t1, MIPS_COP_0_TLB_HI		# Mark entry high as invalid

	dmtc0	zero, MIPS_COP_0_TLB_LO0	# Zero out low entry.
	dmtc0	zero, MIPS_COP_0_TLB_LO1	# Zero out low entry.
	nop
	tlbwi
	nop
	nop
	nop
	nop
1:
	dmtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips3_TLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBUpdate --
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 *	mips3_TLBUpdate(virpageadr, lowregx)
 *		unsigned virpageadr, lowregx;
 *
 * Results:
 *	< 0 if loaded >= 0 if updated.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBUpdate)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	and	t1, a0, 0x1000			# t1 = Even/Odd flag
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Save current PID
	dmtc0	a0, MIPS_COP_0_TLB_HI		# Init high reg
	and	a2, a1, PG_G			# Copy global bit
	nop
	nop
	tlbp					# Probe for the entry.
	dsll	a1, a1, 34
	dsrl	a1, a1, 34
	bne	t1, zero, 2f			# Decide even odd
	mfc0	v0, MIPS_COP_0_TLB_INDEX	# See what we got
# EVEN
	nop
	bltz	v0, 1f				# index < 0 => !found
	nop

	tlbr					# update, read entry first
	nop
	nop
	nop
	dmtc0	a1, MIPS_COP_0_TLB_LO0		# init low reg0.
	nop
	tlbwi					# update slot found
	b	4f
	nop
1:
	mtc0	zero, MIPS_COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, MIPS_COP_0_TLB_HI		# init high reg.
	dmtc0	a1, MIPS_COP_0_TLB_LO0		# init low reg0.
	dmtc0	a2, MIPS_COP_0_TLB_LO1		# init low reg1.
	nop
	tlbwr					# enter into a random slot
	b	4f
	nop
# ODD
2:
	nop
	bltz	v0, 3f				# index < 0 => !found
	nop

	tlbr					# read the entry first
	nop
	nop
	nop
	dmtc0	a1, MIPS_COP_0_TLB_LO1		# init low reg1.
	nop
	tlbwi					# update slot found
	b	4f
	nop
3:
	mtc0	zero, MIPS_COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, MIPS_COP_0_TLB_HI		# init high reg.
	dmtc0	a2, MIPS_COP_0_TLB_LO0		# init low reg0.
	dmtc0	a1, MIPS_COP_0_TLB_LO1		# init low reg1.
	nop
	tlbwr					# enter into a random slot

4:						# Make shure pipeline
	nop					# advances before we
	nop					# uses the tlb.
	nop
	nop
	dmtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
END(mips3_TLBUpdate)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBReadVPS --
 *
 *	Read the TLB entry, including variable-page-size mask.
 *
 *	mips3_TLBReadVPS(entry, tlb)
 *		unsigned entry;
 *		struct tlb *tlb;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tlb will contain the TLB entry found.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBReadVPS)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	nop
	nop
	nop
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID

	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr					# Read from the TLB
	nop
	nop
	nop
	mfc0	t2, MIPS_COP_0_TLB_PG_MASK	# fetch the hi entry
	dmfc0	t3, MIPS_COP_0_TLB_HI		# fetch the hi entry
	dmfc0	t4, MIPS_COP_0_TLB_LO0		# See what we got
	dmfc0	t5, MIPS_COP_0_TLB_LO1		# See what we got
	dmtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	nop
	nop
	nop					# wait for PID active
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
	sw	t2, 0(a1)
	sw	t3, 4(a1)
	sw	t4, 8(a1)
	j	ra
	sw	t5, 12(a1)
END(mips3_TLBReadVPS)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBRead --
 *
 *	Read the TLB entry.
 *
 *	mips3_TLBRead(entry, tlb)
 *		unsigned entry;
 *		struct tlb *tlb;
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tlb will contain the TLB entry found.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBRead)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	nop
	nop
	nop
	dmfc0	t0, MIPS_COP_0_TLB_HI		# Get current PID

	mtc0	a0, MIPS_COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr					# Read from the TLB
	nop
	nop
	nop
	mfc0	t2, MIPS_COP_0_TLB_PG_MASK	# fetch the hi entry
	dmfc0	t3, MIPS_COP_0_TLB_HI		# fetch the hi entry
	dmfc0	t4, MIPS_COP_0_TLB_LO0		# See what we got
	dmfc0	t5, MIPS_COP_0_TLB_LO1		# See what we got
	dmtc0	t0, MIPS_COP_0_TLB_HI		# restore PID
	nop
	nop
	nop					# wait for PID active
	mtc0	v1, MIPS_COP_0_STATUS_REG	# Restore the status register
	sw	t2, 0(a1)
	sw	t3, 4(a1)
	sw	t4, 8(a1)
	j	ra
	sw	t5, 12(a1)
END(mips3_TLBRead)

/*--------------------------------------------------------------------------
 *
 * mips3_TLBGetPID --
 *
 *	mips3_TLBGetPID()
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips3_TLBGetPID)
	dmfc0	v0, MIPS_COP_0_TLB_HI		# get PID
	j	ra
	and	v0, v0, MIPS3_TLB_PID		# mask off PID
END(mips3_TLBGetPID)



/*----------------------------------------------------------------------------
 *
 *	R4000 cache sizing and flushing code.
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 * mips3_ConfigCache --
 *
 *	Size the caches.
 *	NOTE: should only be called from mach_init().
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The size of the data cache is stored into machPrimaryDataCacheSize.
 *	The size of instruction cache is stored into machPrimaryInstCacheSize.
 *	Alignment mask for cache aliasing test is stored in machCacheAliasMask.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_ConfigCache)
	mfc0	v0, MIPS_COP_0_CONFIG		# Get configuration register
	nop
	srl	t1, v0, 9			# Get I cache size.
	and	t1, 3
	li	t2, 4096
	sllv	t2, t2, t1
	sw	t2, machPrimaryDataCacheSize
	addiu	t2, -1
	and	t2, ~(NBPG - 1)
	sw	t2, machCacheAliasMask

	and	t2, v0, 0x20
	srl	t2, t2, 1
	addu	t2, t2, 16
	sw	t2, machPrimaryDataCacheLSize

	srl	t1, v0, 6			# Get I cache size.
	and	t1, 3
	li	t2, 4096
	sllv	t2, t2, t1
	sw	t2, machPrimaryInstCacheSize

	and	t2, v0, 0x10
	addu	t2, t2, 16
	sw	t2, machPrimaryInstCacheLSize
	j	ra
	nop
END(mips3_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * mips3_FlushCache --
 *
 *	Flush the caches. Assumes a line size of 16 bytes for speed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the caches is flushed.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_FlushCache)
	lw	t1, machPrimaryInstCacheSize
	lw	t2, machPrimaryDataCacheSize
 #	lw	t3, machPrimaryInstCacheLSize
 #	lw	t4, machPrimaryDataCacheLSize
/*
 * Flush the instruction cache.
 */
	li	t0, MIPS_KSEG0_START
	addu	t1, t0, t1		# End address
	subu	t1, t1, 128
1:
	cache	0, 0(t0)
	cache	0, 16(t0)
	cache	0, 32(t0)
	cache	0, 48(t0)
	cache	0, 64(t0)
	cache	0, 80(t0)
	cache	0, 96(t0)
	cache	0, 112(t0)
	bne	t0, t1, 1b
	addu	t0, t0, 128

/*
 * Flush the data cache.
 */
	li	t0, MIPS_KSEG0_START
	addu	t1, t0, t2		# End address
	subu	t1, t1, 128
1:
	cache	1, 0(t0)
	cache	1, 16(t0)
	cache	1, 32(t0)
	cache	1, 48(t0)
	cache	1, 64(t0)
	cache	1, 80(t0)
	cache	1, 96(t0)
	cache	1, 112(t0)
	bne	t0, t1, 1b
	addu	t0, t0, 128

	j	ra
	nop
END(mips3_FlushCache)

/*----------------------------------------------------------------------------
 *
 * mips3_FlushICache --
 *
 *	void mips3_FlushICache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush instruction cache for range of addr to addr + len - 1.
 *	The address can be any valid address so long as no TLB misses occur.
 *	Assumes a cache line size of 16 bytes for speed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is flushed.
 *	Must not touch v0.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_FlushICache)
	addu	a1, 127		# Align
	srl	a1, a1, 7	# Number of unrolled loops
1:
	cache	0, 0(a0)
	cache	0, 16(a0)
	cache	0, 32(a0)
	cache	0, 48(a0)
	cache	0, 64(a0)
	cache	0, 80(a0)
	cache	0, 96(a0)
	cache	0, 112(a0)
	addu	a1, -1
	bne	a1, zero, 1b
	addu	a0, 128

	j	ra
	nop
END(mips3_FlushICache)

/*----------------------------------------------------------------------------
 *
 * mips3_FlushDCache --
 *
 *	void mips3_FlushDCache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush data cache for index range of addr to addr + len - 1.
 *	The address is reduced to a kseg0 index.
 *	
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_FlushDCache)
	lw	a2, machPrimaryDataCacheSize
	addiu	a2, -1
	and	a0, a0, a2
	addu	a1, 127		# Align
	li	a2, 0x80000000
	addu	a0, a0, a2
	addu	a1, a1, a0
	and	a0, a0, -128
	subu	a1, a1, a0
	srl	a1, a1, 7	# Compute number of cache lines
1:
	cache	1, 0(a0)
	cache	1, 16(a0)
	cache	1, 32(a0)
	cache	1, 48(a0)
	cache	1, 64(a0)
	cache	1, 80(a0)
	cache	1, 96(a0)
	cache	1, 112(a0)
	addu	a1, -1
	bne	a1, zero, 1b
	addu	a0, 128

	j	ra
	nop
END(mips3_FlushDCache)

/*----------------------------------------------------------------------------
 *
 * mips3_HitFlushDCache --
 *
 *	void mips3_HitFlushDCache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush data cache for range of addr to addr + len - 1.
 *	The address can be any valid viritual address as long
 *	as no TLB invalid traps occur. Only lines with matching
 *	addr is flushed.
 *	
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_HitFlushDCache)
ALEAF(MachHitFlushDCache) /* XXX */
	beq	a1, zero, 2f
	addu	a1, 127		# Align
	addu	a1, a1, a0
	and	a0, a0, -128
	subu	a1, a1, a0
	srl	a1, a1, 7	# Compute number of cache lines
1:
	cache	0x15, 0(a0)
	cache	0x15, 16(a0)
	cache	0x15, 32(a0)
	cache	0x15, 48(a0)
	cache	0x15, 64(a0)
	cache	0x15, 80(a0)
	cache	0x15, 96(a0)
	cache	0x15, 112(a0)
	addu	a1, -1
	bne	a1, zero, 1b
	addu	a0, 128

2:
	j	ra
	nop
END(mips3_HitFlushDCache)
/*----------------------------------------------------------------------------
 *
 * mips3_InvalidateDCache --
 *
 *	void mips3_FlushDCache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Flush data cache for range of addr to addr + len - 1.
 *	The address can be any valid address as long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses or mapped addresses)
 * Results:
 *	None.
 *
 * Side effects:
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(mips3_InvalidateDCache)
	addu	a1, a1, a0			# compute ending address
1:
	addu	a0, a0, 4
	bne	a0, a1, 1b
	cache	0x11,-4(a0)

	j	ra
	nop
END(mips3_InvalidateDCache)

/*----------------------------------------------------------------------------
 *
 *			XXX  END of r4000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */
