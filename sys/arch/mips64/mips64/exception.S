/*	$OpenBSD: exception.S,v 1.6 2004/09/19 20:24:38 pefo Exp $ */

/*
 * Copyright (c) 2002-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

/*
 *  This code handles exceptions and dispatches to the
 *  correct handler depending on the exception type.
 *
 *  Exceptions are directed to the following addresses:
 *      0xffffffffbfc00000  Reset, NMI etc. Not handled by the kernel.
 *	0xffffffff80000000  TLB refill, not in exception.
 *	0xffffffff80000080  XTLB refill, not in exception.
 *	0xffffffffa0000100  Cache errors.
 *	0xffffffff80000180  Interrupts. Same as next.
 *	0xffffffff80000180  Everything else...
 */

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>

#include "assym.h"

/*
 *  DDB stack backtrace uses 'jr ra' to find functions starts.
 *  Put this after functions which does not end with 'jr ra'.
 */
#define	DDB_BARRIER \
	jr ra; nop

	.set	mips3

k_exception_table:
	PTR_VAL	k_intr
	PTR_VAL	k_general
	PTR_VAL	k_tlb_inv
	PTR_VAL	k_tlb_inv
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general

u_exception_table:
	PTR_VAL	u_intr
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general
	PTR_VAL	u_general

	.set	noreorder		# Noreorder is default style!

#define	ITLBNOPFIX	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

/*---------------------------------------------------------------- exception
 *	General exception handler dispatcher. This code is copied
 *	to the vector area and must thus be PIC and less than 128
 *	bytes long to fit. Only k0 and k1 may be used at this time.
 */
	.globl	exception
exception:
	.set	noat
#ifdef TLB_TRACE
	dmfc0	k0, COP_0_EXC_PC
	PTR_L	k1, tlbtrcptr
	PTR_S	k0, 0(k1)
	dmfc0	k0, COP_0_BAD_VADDR
        PTR_S   k0, REGSZ(k1)
	mfc0	k0, COP_0_CAUSE_REG
        PTR_S	k0, 2*REGSZ(k1)
	mfc0	k0, COP_0_STATUS_REG
        PTR_S	k0, 3*REGSZ(k1)

        PTR_L   k1, tlbtrcptr
        PTR_ADDU k1, 4*REGSZ
        LI      k0, 0x100
        nor     k0, zero, k0
        and     k1, k0
        LA      k0, tlbtrcptr
        PTR_S   k1, 0(k0)
#endif
	mfc0	k0, COP_0_STATUS_REG
	mfc0	k1, COP_0_CAUSE_REG
	and	k0, k0, SR_KSU_USER
	beqz	k0, k_exception		# Kernel mode mode
	and	k1, k1, CR_EXC_CODE

	LA	k0, u_exception_table
	PTR_ADDU k0, k0, k1
#ifdef __LP64__
	PTR_ADDU k0, k0, k1		# yes, twice...
#endif
	PTR_L	k0, 0(k0)
	j	k0
	nop

k_exception:
	LA	k0, k_exception_table
	PTR_ADDU k0, k0, k1
#ifdef __LP64__
	PTR_ADDU k0, k0, k1		# yes, twice...
#endif
	PTR_L	k0, 0(k0)
	j	k0
	nop
	.set	at
	.globl	e_exception
e_exception:

	DDB_BARRIER

/*---------------------------------------------------------------- k_intr
 *	Handle an interrupt in kernel mode. This is easy since we
 *	just need to save away the 'save' registers and state.
 *	State is saved on kernel stack.
 */

NNON_LEAF(k_intr, FRAMESZ(KERN_EXC_FRAME_SIZE), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(KERN_EXC_FRAME_SIZE))
	PTR_SUB	k0, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	SAVE_CPU(k0, CF_RA_OFFS)
#if 0
	cfc0	v1, COP_0_ICR
	SAVE_REG(v1, IC, k0, CF_RA_OFFS)
#endif
	.set	at
	move	sp, k0			# Already on kernel stack
	LA	gp, _gp
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	PTR_S	a0, 0(sp)
	jal	interrupt
	PTR_S	a3, CF_RA_OFFS + KERN_REG_SIZE(sp)

	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG

	PTR_L	a0, CF_RA_OFFS + KERN_REG_SIZE(sp)
	.set	noat
#if 0
	RESTORE_REG(t0, IC, sp, CF_RA_OFFS)
	ctc0	t0, COP_0_ICR
#endif
	RESTORE_CPU(sp, CF_RA_OFFS)
	PTR_ADDU sp, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	sync
	eret
	DDB_BARRIER
	.set	at
END(k_intr)

/*---------------------------------------------------------------- u_intr
 *	Handle an interrupt in user mode. Save the relevant user
 *	registers into the u.u_pcb struct. This will allow us
 *	to preempt the interrupted process. Full save is held
 *	off though until a switch() really is requiered.
 */
NNON_LEAF(u_intr, FRAMESZ(CF_SZ), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_L	k0, curprocpaddr
	SAVE_CPU(k0, 0)
#if 0
	cfc0	v1, COP_0_ICR
	SAVE_REG(v1, IC, k0, 0)
#endif
	PTR_ADDU sp, k0, USPACE-FRAMESZ(CF_SZ)
	LA	gp, _gp
	.set	at
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	PTR_S	a0, 0(sp)
	jal	interrupt
	PTR_S	a3, CF_RA_OFFS(sp)		# for debugging

	lw	v0, astpending		# any pending interrupts?
	beq	v0, zero, 4f
	nop

	PTR_L	t0, curprocpaddr
	SAVE_CPU_SREG(t0, 0)

#ifdef PERFCNTRS
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf save.

	mfc0	v0, COP_0_PC_CTRL
	PTR_L	t0, curproc
	sw	v0, P_PC_CTRL(t0)
	dmfc0	v0, COP_0_WATCH_1
	dmfc0	v1, COP_0_WATCH_2
	sd	v0, P_WATCH_1(t0)
	sd	v1, P_WATCH_2(t0)
	mfc0	v0, COP_0_WATCH_M
	mfc0	v1, COP_0_PC_COUNT
	sw	v0, P_WATCH_M(t0)
	sw	v1, P_PC_COUNT(t0)
	mtc0	zero, COP_0_PC_CTRL
	dmtc0	zero, COP_0_WATCH_1
	dmtc0	zero, COP_0_WATCH_2
	nop;nop;nop;nop
1:
#endif
	jal	softintr
	nop
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
#ifdef PERFCNTRS
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf setup.

	PTR_L	t1, curproc		# set up rm7k.
	ld	v0, P_WATCH_1(t1)
	dmtc0	v0, COP_0_WATCH_1
	ld	v0, P_WATCH_2(t1)
	dmtc0	v0, COP_0_WATCH_2
	lw	v0, P_WATCH_M(t1)
	mtc0	v0, COP_0_WATCH_M
	lw	v0, P_PC_CTRL(t1)
	lw	v1, P_PC_COUNT(t1)
	nop;nop
	mtc0	v0, COP_0_PC_CTRL
	nop;nop;nop;nop
	mtc0	v1, COP_0_PC_COUNT
	nop;nop;nop;nop
1:
#endif
	PTR_L	t0, curprocpaddr
	RESTORE_CPU_SREG(t0, 0)

4:
	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	ori	t0, SR_EXL		# restoring to user mode.
	mtc0	t0, COP_0_STATUS_REG	# must set exeption level bit.
	ITLBNOPFIX

	PTR_L	k0, curprocpaddr
	RESTORE_REG(a3, CPL, k0, 0)
	sw	a3, cpl
	.set	noat
	RESTORE_REG(a0, PC, k0, 0)
#if 0
	RESTORE_REG(t0, IC, k0, 0)
	ctc0	t0, COP_0_ICR
#endif
	RESTORE_CPU(k0, 0)
	RESTORE_REG(sp, SP, k0, 0)
	LI	k0, 0
	LI	k1, 0
	sync
	eret
	DDB_BARRIER
	.set	at
END(u_intr)

/*---------------------------------------------------------------- set_sint
 *  Atomic ipending update
 */
LEAF(set_sint, 0)
	LA	v1, ipending
1:
	ll	v0, 0(v1)
	or	v0, a0
	sc	v0, 0(v1)
	beqz	v0, 1b
	nop
	j	ra
	nop
END(set_sint)

/*---------------------------------------------------------------- k_general
 *	Handle a kernel general trap. This is very much like
 *	k_intr except that we call ktrap instead of interrupt.
 */

NNON_LEAF(k_general, FRAMESZ(KERN_EXC_FRAME_SIZE), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(KERN_EXC_FRAME_SIZE))
	PTR_SUB	k0, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	SAVE_CPU(k0, CF_RA_OFFS)
#if 0
	cfc0	v1, COP_0_ICR
	SAVE_REG(v1, IC, k0, CF_RA_OFFS)
#endif
#if defined(DDB)
	SAVE_CPU_SREG(k0, CF_RA_OFFS)
#endif
	.set	at
	move	sp, k0			# Already on kernel stack
	LA	gp, _gp
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX
	PTR_S	a0, 0(sp)
	jal	trap
	PTR_S	a3, CF_RA_OFFS + KERN_REG_SIZE(sp)

	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG

	move	a0, v0
	.set	noat
#if 0
	RESTORE_REG(t0, IC, sp, CF_RA_OFFS)
	ctc0	t0, COP_0_ICR
#endif
	RESTORE_CPU(sp, CF_RA_OFFS)
	PTR_ADDU sp, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	sync
	eret
	DDB_BARRIER
	.set	at
END(k_general)

/*---------------------------------------------------------------- u_general
 *	Handle a user general trap.
 */
NNON_LEAF(u_general, FRAMESZ(CF_SZ), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))

	PTR_L	k0, curprocpaddr
	SAVE_CPU(k0, 0)
#if 0
	cfc0	v1, COP_0_ICR
	SAVE_REG(v1, IC, k0, 0)
#endif
	SAVE_CPU_SREG(k0, 0)
	PTR_ADDU sp, k0, USPACE-FRAMESZ(CF_SZ)
	LA	gp, _gp
	.set	at
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

#ifdef PERFCNTRS
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf save.

	mfc0	v0, COP_0_PC_CTRL
	PTR_L	t0, curproc
	sw	v0, P_PC_CTRL(t0)
	dmfc0	v0, COP_0_WATCH_1
	dmfc0	v1, COP_0_WATCH_2
	sd	v0, P_WATCH_1(t0)
	sd	v1, P_WATCH_2(t0)
	mfc0	v0, COP_0_WATCH_M
	mfc0	v1, COP_0_PC_COUNT
	sw	v0, P_WATCH_M(t0)
	sw	v1, P_PC_COUNT(t0)
	mtc0	zero, COP_0_PC_CTRL
	nop;nop;nop;nop
1:
#endif

	jal	trap
	PTR_S	a3, CF_RA_OFFS(sp)		# for debugging

#ifdef PERFCNTRS
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf setup.

	LOAD	t0, curproc		# set up rm7k.
	ld	v0, P_WATCH_1(t0)
	dmtc0	v0, COP_0_WATCH_1
	ld	v0, P_WATCH_2(t0)
	dmtc0	v0, COP_0_WATCH_2
	lw	v0, P_WATCH_M(t0)
	mtc0	v0, COP_0_WATCH_M
	lw	v0, P_PC_CTRL(t0)
	lw	v1, P_PC_COUNT(t0)
	nop;nop
	mtc0	v0, COP_0_PC_CTRL
	nop;nop;nop;nop
	mtc0	v1, COP_0_PC_COUNT
	nop;nop;nop;nop
1:
#endif

4:
	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	ori	t0, SR_EXL		# restoring to user mode.
	mtc0	t0, COP_0_STATUS_REG	# must set exeption level bit.
	ITLBNOPFIX

	PTR_L	k0, curprocpaddr
	RESTORE_REG(a3, CPL, k0, 0)
	sw	a3, cpl
	.set	noat
	RESTORE_CPU_SREG(k0, 0)
	RESTORE_REG(a0, PC, k0, 0)
#if 0
	RESTORE_REG(t0, IC, k0, 0)
	ctc0	t0, COP_0_ICR
#endif
	RESTORE_CPU(k0, 0)
	RESTORE_REG(sp, SP, k0, 0)
	LI	k0, 0
	LI	k1, 0
	sync
	eret
	DDB_BARRIER
	.set	at
END(u_general)

#ifdef notyet
/*---------------------------------------------------------------- u_syscall
 *	Syscall exceptions are special such that they can be
 *	optimized by not saving more than what is really needed.
 *	Syscalls are actually 'function calls' from the user
 *	programs point of view and thus it does not expect us to
 *	save away all temporary registers etc. Just save state and
 *	args to avoid a lot of overhead.
 */
NNON_LEAF(u_syscall, FRAMESZ(CF_SZ), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))

	REG_S	a0, UADDR+U_PCB_REGS+(A0 * REGSZ)
	REG_S	a1, UADDR+U_PCB_REGS+(A1 * REGSZ)
	REG_S	a2, UADDR+U_PCB_REGS+(A2 * REGSZ)
	REG_S	a3, UADDR+U_PCB_REGS+(A3 * REGSZ)
	mfc0	a0, COP_0_STATUS_REG	# First arg is the status reg.
	mfc0	a1, COP_0_CAUSE_REG	# Second arg is the cause reg.
	dmfc0	a3, COP_0_EXC_PC		# Fourth arg is the pc.
	REG_S	sp, UADDR+U_PCB_REGS+(SP * REGSZ)
	LA	sp, KERNELSTACK - FRAMESZ(CF_SZ)	# switch to kernel SP
	REG_S	ra, UADDR+U_PCB_REGS+(RA * REGSZ)
	REG_S	a0, UADDR+U_PCB_REGS+(SR * REGSZ)
	REG_S	a1, UADDR+U_PCB_REGS+(CAUSE * REGSZ)
	REG_S	a3, UADDR+U_PCB_REGS+(PC * REGSZ)
	REG_S	a3, CF_RA_OFFS(sp)		# for debugging
	LA	gp, _gp				# switch to kernel GP
	lw	a3, cpl
	sw	a3, UADDR+U_PCB_REGS+(CPL * REGSZ)
	.set	at
# Turn off fpu and enter kernel mode
	and	t0, a0, ~(SR_COP_1_BIT | SR_EXL | SR_KSU_MASK | SR_INT_ENAB)
	mtc0	t0, COP_0_STATUS_REG
	li	a0, UADDR+U_PCB_REGS
	ITLBNOPFIX
/*
 *  If CPU is a RM7000 save away performance stuff.
 */
#if 0
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf save.
	mfc0	v0, COP_0_PC_CTRL
	lw      t0, curproc
	sw	v0, P_PC_CTRL(t0)
	dmfc0	v0, COP_0_WATCH_1
	dmfc0	v1, COP_0_WATCH_2
	sd	v0, P_WATCH_1(t0)
	sd	v1, P_WATCH_2(t0)
	mfc0	v0, COP_0_WATCH_M
	mfc0	v1, COP_0_PC_COUNT
	sw	v0, P_WATCH_M(t0)
	sw	v1, P_PC_COUNT(t0)
	mtc0	zero, COP_0_PC_CTRL
	dmtc0	zero, COP_0_WATCH_1
	dmtc0	zero, COP_0_WATCH_2
1:
#endif

	jal	trap
	nop

	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	ori	t0, SR_EXL
	mtc0	t0, COP_0_STATUS_REG	# set exeption level
	ITLBNOPFIX

#if 0
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do perf setup.

	PTR_L	t0, curproc		# set up rm7k.
	ld	v0, P_WATCH_1(t0)
	dmtc0	v0, COP_0_WATCH_1
	ld	v0, P_WATCH_2(t0)
	dmtc0	v0, COP_0_WATCH_2
	lw	v0, P_WATCH_M(t0)
	mtc0	v0, COP_0_WATCH_M
	lw	v0, P_PC_CTRL(t0)
	lw	v1, P_PC_COUNT(t0)
	nop;nop
	mtc0	v0, COP_0_PC_CTRL
	nop;nop;nop;nop
	mtc0	v1, COP_0_PC_COUNT
	nop;nop;nop;nop
1:
#endif
	lw	a3, UADDR+U_PCB_REGS+(CPL * REGSZ)
	sw	a3, cpl

	.set	noat

	REG_L	a0, UADDR+U_PCB_REGS+(SR * REGSZ)
	mtc0	a0, COP_0_STATUS_REG	# still exeption level
	REG_L	a0, UADDR+U_PCB_REGS+(PC * REGSZ)
	REG_L	v0, UADDR+U_PCB_REGS+(V0 * REGSZ)
	dmtc0	a0, COP_0_EXC_PC		# set return address
	REG_L	v1, UADDR+U_PCB_REGS+(V1 * REGSZ)
	REG_L	gp, UADDR+U_PCB_REGS+(GP * REGSZ)
	REG_L	sp, UADDR+U_PCB_REGS+(SP * REGSZ)
	REG_L	ra, UADDR+U_PCB_REGS+(RA * REGSZ)
	sync
	eret
	DDB_BARRIER
	.set	at
END(u_syscall)
#endif


/*-------------------------------------------------------------- proc_trampoline
 *	Setup for and return to user.
 */
LEAF(proc_trampoline, 0)
	sw	zero, cpl		# lower to spl0
	lw	t0, ipending
	beq	t0, zero, 0f
	nop

	jal	setsoftintr0		# process any pending ints
	nop
0:
	jal	s0
	move	a0,s1			# set up for return to user.

#if 0
	lw	t0, cpu_is_rm7k
	beqz	t0, 1f			# not an RM7K. Don't do IC reg.

	LOAD	t0, curproc		# set up rm7k.
	ld	v0, P_WATCH_1(t0)
	dmtc0	v0, COP_0_WATCH_1
	ld	v0, P_WATCH_2(t0)
	dmtc0	v0, COP_0_WATCH_2
	lw	v0, P_WATCH_M(t0)
	mtc0	v0, COP_0_WATCH_M
	lw	v0, P_PC_CTRL(t0)
	lw	v1, P_PC_COUNT(t0)
	nop;nop
	mtc0	v0, COP_0_PC_CTRL
	nop;nop;nop;nop
	mtc0	v1, COP_0_PC_COUNT
	nop;nop;nop;nop
	li	v0, IC_INT_PERF
	ctc0	v0, COP_0_ICR		# enable perfcntr interrupt.
1:
#endif
	mfc0	t0, COP_0_STATUS_REG	# dis int preserve settings.
	li	t1, ~SR_INT_ENAB
	and	t0, t0, t1
	mtc0	t0, COP_0_STATUS_REG
	ITLBNOPFIX

	ori	t0, SR_EXL		# restoring to user mode.
	mtc0	t0, COP_0_STATUS_REG	# must set exeption level bit.
	ITLBNOPFIX

	.set	noat
	PTR_L	k0, curprocpaddr
	RESTORE_CPU_SREG(k0, 0)
	RESTORE_REG(a0, PC, k0, 0)
#if 0
	RESTORE_REG(t0, IC, k0, 0)
	ctc0	t0, COP_0_ICR
#endif
	RESTORE_CPU(k0, 0)
	RESTORE_REG(sp, SP, k0, 0)
	LI	k0, 0
	LI	k1, 0
	sync
	eret
	.set	at
END(proc_trampoline)
