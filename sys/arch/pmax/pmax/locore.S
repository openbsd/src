/*	$NetBSD: locore.S,v 1.26 1996/11/06 20:19:38 cgd Exp $	*/

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
 *	Contains code that is the first executed at boot time plus
 *	assembly language support routines.
 */

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/regnum.h>
#include <machine/asm.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/pte.h>

/*----------------------------------------------------------------------------
 *
 *  Macros used to save and restore registers when entering and
 *  exiting the kernel.  When coming from user space, we need
 *  to initialize the kernel stack and on leaving, return to the
 *  user's stack.
 *  When coming from kernel-space we just save and restore from the
 *  kernel stack set up by a previous user-mode exception.
 *
 *----------------------------------------------------------------------------
 */


/*
 * Restore saved user registers before returning
 * to user-space after an exception or TLB miss.
 *
 * XXX	we don't restore all the regs, because the r3000 and r4000
 *     	use different mechanisms to set up the return address.
 *     	the current Pica code uses v1 for that, so we leave
 *     	it up to the caller of this macro to restore AT and v0.
 *	Don't ask why, I don't know.
 */
#define RESTORE_USER_REGS(saveaddr)			\
	lw	v1, saveaddr+U_PCB_REGS+(V1 * 4)		; \
	lw	a0, saveaddr+U_PCB_REGS+(A0 * 4)		; \
	lw	a1, saveaddr+U_PCB_REGS+(A1 * 4)		; \
	lw	a2, saveaddr+U_PCB_REGS+(A2 * 4)		; \
	lw	a3, saveaddr+U_PCB_REGS+(A3 * 4)		; \
	lw	t0, saveaddr+U_PCB_REGS+(T0 * 4)		; \
	lw	t1, saveaddr+U_PCB_REGS+(T1 * 4)		; \
	lw	t2, saveaddr+U_PCB_REGS+(T2 * 4)		; \
	lw	t3, saveaddr+U_PCB_REGS+(T3 * 4)		; \
	lw	t4, saveaddr+U_PCB_REGS+(T4 * 4)		; \
	lw	t5, saveaddr+U_PCB_REGS+(T5 * 4)		; \
	lw	t6, saveaddr+U_PCB_REGS+(T6 * 4)		; \
	lw	t7, saveaddr+U_PCB_REGS+(T7 * 4)		; \
	lw	s0, saveaddr+U_PCB_REGS+(S0 * 4)		; \
	lw	s1, saveaddr+U_PCB_REGS+(S1 * 4)		; \
	lw	s2, saveaddr+U_PCB_REGS+(S2 * 4)		; \
	lw	s3, saveaddr+U_PCB_REGS+(S3 * 4)		; \
	lw	s4, saveaddr+U_PCB_REGS+(S4 * 4)		; \
	lw	s5, saveaddr+U_PCB_REGS+(S5 * 4)		; \
	lw	s6, saveaddr+U_PCB_REGS+(S6 * 4)		; \
	lw	s7, saveaddr+U_PCB_REGS+(S7 * 4)		; \
	lw	t8, saveaddr+U_PCB_REGS+(T8 * 4)		; \
	lw	t9, saveaddr+U_PCB_REGS+(T9 * 4)		; \
	lw	gp, saveaddr+U_PCB_REGS+(GP * 4)		; \
	lw	sp, saveaddr+U_PCB_REGS+(SP * 4)		; \
	lw	s8, saveaddr+U_PCB_REGS+(S8 * 4)		; \
	lw	ra, saveaddr+U_PCB_REGS+(RA * 4)


/*
 * Restore call-used registers(?) before returning
 * to the previous  kernel stackframe after a  after an exception or 
 * TLB miss from kernel space.
 *
 * XXX	we don't restore all the regs, because the r3000 and r4000
 *     	use different mechanisms to set up the return address.
 *     	the current Pica code uses v1 for that, so we leave
 *     	it up to the caller of this macro to restore AT and v0.
 *	Don't ask why, I don't konw.
 */
#define	RESTORE_KERN_REGISTERS(offset)			\
	lw	v1, offset + (V1 * 4)(sp)		; \
	lw	a0, offset + (A0 * 4)(sp)		; \
	lw	a1, offset + (A1 * 4)(sp)		; \
	lw	a2, offset + (A2 * 4)(sp)		; \
	lw	a3, offset + (A3 * 4)(sp)		; \
	lw	t0, offset + (T0 * 4)(sp)		; \
	lw	t1, offset + (T1 * 4)(sp)		; \
	lw	t2, offset + (T2 * 4)(sp)		; \
	lw	t3, offset + (T3 * 4)(sp)		; \
	lw	t4, offset + (T4 * 4)(sp)		; \
	lw	t5, offset + (T5 * 4)(sp)		; \
	lw	t6, offset + (T6 * 4)(sp)		; \
	lw	t7, offset + (T7 * 4)(sp)		; \
	lw	t8, offset + (T8 * 4)(sp)		; \
	lw	t9, offset + (T9 * 4)(sp)		; \
	lw	ra, offset + (RA * 4)(sp)



/*----------------------------------------------------------------------------
 *
 * start -- boostrap kernel entry point
 *
 *----------------------------------------------------------------------------
 */
#include "assym.h"

	.set	noreorder

/*
 * Amount to take off of the stack for the benefit of the debugger.
 */
#define START_FRAME	((4 * 4) + 4 + 4)

	.globl	start
	.globl	_C_LABEL(kernel_text)
	_C_LABEL(kernel_text) = start
start:
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable interrupts
	li	t1, MIPS_KSEG0_START	# invalid address
	mtc0	t1, MIPS_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MIPS_COP_0_TLB_LOW	# Zero out low entry.
/*
 * Clear the TLB (just to be safe).
 * Align the starting value (t1), the increment (t2) and the upper bound (t3).
 */
	move	t1, zero
	li	t2, 1 << MIPS1_TLB_INDEX_SHIFT
	li	t3, MIPS1_TLB_NUM_TLB_ENTRIES << MIPS1_TLB_INDEX_SHIFT
1:
	mtc0	t1, MIPS_COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, t2			# Increment index.
	bne	t1, t3, 1b			# NB: always executes next
	tlbwi					# Write the TLB entry.

/*
 * Initialize stack and call machine startup.
 */
	la	sp, start - START_FRAME
#ifdef __GP_SUPPORT__
	la	gp, _C_LABEL(_gp)
#endif
	sw	zero, START_FRAME - 4(sp)	# Zero out old ra for debugger
	jal	_C_LABEL(mach_init)		# mach_init(argc, argv, envp)
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger

	li	t0, MIPS_SR_COP_1_BIT		# Disable interrupts and
	mtc0	t0, MIPS_COP_0_STATUS_REG	#   enable the fp coprocessor
	li	sp, KERNELSTACK - START_FRAME	# switch to standard stack
	mfc0	t0, MIPS_COP_0_PRID		# read processor ID register

	nop					# XXX r4000 pipeline:
	nop					# wait for new status to
	nop					# to be effective
	nop
	cfc1	t1, MIPS_FPU_ID			# read FPU ID register
	sw	t0, _C_LABEL(cpu_id)		# save PRID register
	sw	t1, _C_LABEL(fpu_id)		# save FPU ID register
	jal	_C_LABEL(main)			# main(regs)
	move	a0, zero
	PANIC("Startup failed!")
/*
 * Setup for and return to user.
 */
LEAF(proc_trampoline)
	jal	s0
	move	a0,s1				# set up for return to user.

	.set	noat
	lw	k0, UADDR+U_PCB_REGS+(SR * 4)
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	k0, MIPS_COP_0_STATUS_REG	# switch to user mode
	mtlo	t0
	mthi	t1
	lw	k0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	lw	v1, UADDR+U_PCB_REGS+(V1 * 4)
	lw	a0, UADDR+U_PCB_REGS+(A0 * 4)
	lw	a1, UADDR+U_PCB_REGS+(A1 * 4)
	lw	a2, UADDR+U_PCB_REGS+(A2 * 4)
	lw	a3, UADDR+U_PCB_REGS+(A3 * 4)
	lw	t0, UADDR+U_PCB_REGS+(T0 * 4)
	lw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	lw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	lw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	lw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	lw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	lw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	lw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	lw	s0, UADDR+U_PCB_REGS+(S0 * 4)
	lw	s1, UADDR+U_PCB_REGS+(S1 * 4)
	lw	s2, UADDR+U_PCB_REGS+(S2 * 4)
	lw	s3, UADDR+U_PCB_REGS+(S3 * 4)
	lw	s4, UADDR+U_PCB_REGS+(S4 * 4)
	lw	s5, UADDR+U_PCB_REGS+(S5 * 4)
	lw	s6, UADDR+U_PCB_REGS+(S6 * 4)
	lw	s7, UADDR+U_PCB_REGS+(S7 * 4)
	lw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	lw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	lw	gp, UADDR+U_PCB_REGS+(GP * 4)
	lw	sp, UADDR+U_PCB_REGS+(SP * 4)
	lw	s8, UADDR+U_PCB_REGS+(S8 * 4)
	lw	ra, UADDR+U_PCB_REGS+(RA * 4)
	j	k0
	rfe
	.set	at
END(proc_trampoline)

/*
 * This code is copied the user's stack for returning from signal handlers
 * (see sendsig() and sigreturn()). We have to compute the address
 * of the sigcontext struct for the sigreturn call.
 */
LEAF(sigcode)
	addu	a0, sp, 16		# address of sigcontext
	li	v0, SYS_sigreturn	# sigreturn(scp)
	syscall
	break	0			# just in case sigreturn fails
ALEAF(esigcode)
END(sigcode)

/*
 * Primitives
 */

/*
 * This table is indexed by u.u_pcb.pcb_onfault in trap().
 * The reason for using this table rather than storing an address in
 * u.u_pcb.pcb_onfault is simply to make the code faster.
 */
	.data
	.align	2
	.globl	_C_LABEL(onfault_table)
_C_LABEL(onfault_table):
	.word	0		# invalid index number
#define BADERR		1
	.word	_C_LABEL(baderr)
#define COPYERR		2
	.word	_C_LABEL(copyerr)
#define FSWBERR		3
	.word	_C_LABEL(fswberr)
#define FSWINTRBERR	4
	.word	_C_LABEL(fswintrberr)
#ifdef KADB
#define KADBERR		5
	.word	_C_LABEL(kadberr)
#endif
	.text

/*
 * See if access to addr with a len type instruction causes a machine check.
 * len is length of access (1=byte, 2=short, 4=long)
 *
 * badaddr(addr, len)
 *	char *addr;
 *	int len;
 */
LEAF(badaddr)
	li	v0, BADERR
	bne	a1, 1, 2f
	sw	v0, UADDR+U_PCB_ONFAULT
	b	5f
	lbu	v0, (a0)
2:
	bne	a1, 2, 4f
	nop
	b	5f
	lhu	v0, (a0)
4:
	lw	v0, (a0)
5:
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero		# made it w/o errors
baderr:
	j	ra
	li	v0, 1			# trap sends us here
END(badaddr)

/*
 * fillw(pat, addr, count)
 */
LEAF(fillw)
1:
	addiu	a2, a2, -1
	sh	a0, 0(a1)
	bne	a2,zero, 1b
	addiu	a1, a1, 2

	jr	ra
	nop
END(fillw)


/*
 * Copy a null terminated string within the kernel address space.
 * Maxlength may be null if count not wanted.
 *	copystr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		size_t maxlength;
 *		size_t *lencopied;
 */
LEAF(copystr)
	move	t2, a2			# Save the number of bytes
1:
	lbu	t0, 0(a0)
	subu	a2, a2, 1
	beq	t0, zero, 2f
	sb	t0, 0(a1)
	addu	a0, a0, 1
	bne	a2, zero, 1b
	addu	a1, a1, 1
2:
	beq	a3, zero, 3f
	subu	a2, t2, a2		# compute length copied
	sw	a2, 0(a3)
3:
	j	ra
	move	v0, zero
END(copystr)

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 *
 *	copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		size_t maxlength;
 *		size_t *lencopied;
 */
NON_LEAF(copyinstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a0, zero, _C_LABEL(copyerr)	# make sure address is in user space
	li	v0, COPYERR
	jal	_C_LABEL(copystr)
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	ra, STAND_RA_OFFSET(sp)
	sw	zero, UADDR+U_PCB_ONFAULT
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyinstr)

/*
 * Copy a null terminated string from the kernel address space into
 * the user address space.
 *
 *	copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		size_t maxlength;
 *		size_t *lencopied;
 */
NON_LEAF(copyoutstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a1, zero, _C_LABEL(copyerr)	# make sure address is in user space
	li	v0, COPYERR
	jal	_C_LABEL(copystr)
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	ra, STAND_RA_OFFSET(sp)
	sw	zero, UADDR+U_PCB_ONFAULT
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyoutstr)

/*
 * Copy specified amount of data from user space into the kernel
 *	copyin(from, to, len)
 *		caddr_t *from;	(user source address)
 *		caddr_t *to;	(kernel destination address)
 *		unsigned len;
 */
NON_LEAF(copyin, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a0, zero, _C_LABEL(copyerr)	# make sure address is in user space
	li	v0, COPYERR
	jal	_C_LABEL(bcopy)
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	ra, STAND_RA_OFFSET(sp)
	sw	zero, UADDR+U_PCB_ONFAULT
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyin)

/*
 * Copy specified amount of data from kernel to the user space
 *	copyout(from, to, len)
 *		caddr_t *from;	(kernel source address)
 *		caddr_t *to;	(user destination address)
 *		unsigned len;
 */
NON_LEAF(copyout, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a1, zero, _C_LABEL(copyerr)	# make sure address is in user space
	li	v0, COPYERR
	jal	_C_LABEL(bcopy)
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	ra, STAND_RA_OFFSET(sp)
	sw	zero, UADDR+U_PCB_ONFAULT
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	move	v0, zero
END(copyout)

LEAF(copyerr)
	lw	ra, STAND_RA_OFFSET(sp)
	sw	zero, UADDR+U_PCB_ONFAULT
	addu	sp, sp, STAND_FRAME_SIZE
	j	ra
	li	v0, EFAULT		# return error
END(copyerr)

/*
 * Copy the kernel stack to the new process and save the current context so
 * the new process will return nonzero when it is resumed by cpu_switch().
 *
 *	copykstack(up)
 *		struct user *up;
 */
LEAF(copykstack)
	subu	v0, sp, UADDR		# compute offset into stack
	addu	v0, v0, a0		# v0 = new stack address
	move	v1, sp			# v1 = old stack address
	li	t1, KERNELSTACK
1:
	lw	t0, 0(v1)		# copy stack data
	addu	v1, v1, 4
	sw	t0, 0(v0)
	bne	v1, t1, 1b
	addu	v0, v0, 4
	/* FALLTHROUGH */
/*
 * Save registers and state so we can do a longjmp later.
 * Note: this only works if p != curproc since
 * cpu_switch() will copy over pcb_context.
 *
 *	savectx(up)
 *		struct user *up;
 */
ALEAF(savectx)
	sw	s0, U_PCB_CONTEXT+0(a0)
	sw	s1, U_PCB_CONTEXT+4(a0)
	sw	s2, U_PCB_CONTEXT+8(a0)
	sw	s3, U_PCB_CONTEXT+12(a0)
	mfc0	v0, MIPS_COP_0_STATUS_REG
	sw	s4, U_PCB_CONTEXT+16(a0)
	sw	s5, U_PCB_CONTEXT+20(a0)
	sw	s6, U_PCB_CONTEXT+24(a0)
	sw	s7, U_PCB_CONTEXT+28(a0)
	sw	sp, U_PCB_CONTEXT+32(a0)
	sw	s8, U_PCB_CONTEXT+36(a0)
	sw	ra, U_PCB_CONTEXT+40(a0)
	sw	v0, U_PCB_CONTEXT+44(a0)
	j	ra
	move	v0, zero
END(copykstack)

/*
 * The following primitives manipulate the run queues.  whichqs tells which
 * of the 32 queues qs have processes in them.  Setrunqueue puts processes
 * into queues, remrunqueue removes them from queues.  The running process is
 * on no queue, other processes are on a queue related to p->p_priority,
 * divided by 4 actually to shrink the 0-127 range of priorities into the 32
 * available queues.
 */
/*
 * setrunqueue(p)
 *	proc *p;
 *
 * Call should be made at splclock(), and p->p_stat should be SRUN.
 */
NON_LEAF(setrunqueue, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	lw	t0, P_BACK(a0)		## firewall: p->p_back must be 0
	sw	ra, STAND_RA_OFFSET(sp)	##
	beq	t0, zero, 1f		##
	lbu	t0, P_PRIORITY(a0)	# put on p->p_priority / 4 queue
	PANIC("setrunqueue")		##
1:
	li	t1, 1			# compute corresponding bit
	srl	t0, t0, 2		# compute index into 'whichqs'
	sll	t1, t1, t0
	lw	t2, _C_LABEL(whichqs)	# set corresponding bit
	nop
	or	t2, t2, t1
	sw	t2, _C_LABEL(whichqs)
	sll	t0, t0, 3		# compute index into 'qs'
	la	t1, _C_LABEL(qs)
	addu	t0, t0, t1		# t0 = qp = &qs[pri >> 2]
	lw	t1, P_BACK(t0)		# t1 = qp->ph_rlink
	sw	t0, P_FORW(a0)		# p->p_forw = qp
	sw	t1, P_BACK(a0)		# p->p_back = qp->ph_rlink
	sw	a0, P_FORW(t1)		# p->p_back->p_forw = p;
	sw	a0, P_BACK(t0)		# qp->ph_rlink = p
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(setrunqueue)

/*
 * remrunqueue(p)
 *
 * Call should be made at splclock().
 */
NON_LEAF(remrunqueue, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	lbu	t0, P_PRIORITY(a0)	# get from p->p_priority / 4 queue
	li	t1, 1			# compute corresponding bit
	srl	t0, t0, 2		# compute index into 'whichqs'
	lw	t2, _C_LABEL(whichqs)	# check corresponding bit
	sll	t1, t1, t0
	and	v0, t2, t1
	sw	ra, STAND_RA_OFFSET(sp)	##
	bne	v0, zero, 1f		##
	lw	v0, P_BACK(a0)		# v0 = p->p_back
	PANIC("remrunqueue")		## it wasnt recorded to be on its q
1:
	lw	v1, P_FORW(a0)		# v1 = p->p_forw
	nop
	sw	v1, P_FORW(v0)		# p->p_back->p_forw = p->p_forw;
	sw	v0, P_BACK(v1)		# p->p_forw->p_back = p->r_rlink
	sll	t0, t0, 3		# compute index into 'qs'
	la	v0, _C_LABEL(qs)
	addu	t0, t0, v0		# t0 = qp = &qs[pri >> 2]
	lw	v0, P_FORW(t0)		# check if queue empty
	nop
	bne	v0, t0, 2f		# No. qp->ph_link != qp
	nop
	xor	t2, t2, t1		# clear corresponding bit in 'whichqs'
	sw	t2, _C_LABEL(whichqs)
2:
	sw	zero, P_BACK(a0)	## for firewall checking
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(remrunqueue)

/*
 * switch_exit()
 *
 * At exit of a process, do a cpu_switch for the last time.
 * The mapping of the pcb at p->p_addr has already been deleted,
 * and the memory for the pcb+stack has been freed.
 * All interrupts should be blocked at this point.
 */
LEAF(switch_exit)
	la	v1, _C_LABEL(nullproc)		# save state into garbage proc
	lw	t0, P_UPTE+0(v1)		# t0 = first u. pte
	lw	t1, P_UPTE+4(v1)		# t1 = 2nd u. pte
	li	v0, UADDR			# v0 = first HI entry
	mtc0	zero, MIPS_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MIPS_COP_0_TLB_HI		# init high entry
	mtc0	t0, MIPS_COP_0_TLB_LOW		# init low entry
	li	t0, 1 << MIPS1_TLB_INDEX_SHIFT
	tlbwi					# Write the TLB entry.
	addu	v0, v0, NBPG			# 2nd HI entry
	mtc0	t0, MIPS_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MIPS_COP_0_TLB_HI		# init high entry
	mtc0	t1, MIPS_COP_0_TLB_LOW		# init low entry
	sw	zero, _C_LABEL(curproc)
	tlbwi					# Write the TLB entry.
	b	_C_LABEL(cpu_switch)
	li	sp, KERNELSTACK - START_FRAME	# switch to standard stack
END(switch_exit)

/*
 * When no processes are on the runq, cpu_switch branches to idle
 * to wait for something to come ready.
 * Note: this is really a part of cpu_switch() but defined here for kernel
 * profiling.
 */
LEAF(idle)
	li	t0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable all interrupts
	sw	zero, _C_LABEL(curproc)		# set curproc NULL for stats
1:
	lw	t0, _C_LABEL(whichqs)		# look for non-empty queue
	nop
	beq	t0, zero, 1b
	nop
	b	sw1
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable all interrupts
END(idle)

/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
NON_LEAF(cpu_switch, STAND_FRAME_SIZE, ra)
	sw	sp, UADDR+U_PCB_CONTEXT+32	# save old sp
	subu	sp, sp, STAND_FRAME_SIZE
	sw	ra, STAND_RA_OFFSET(sp)
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	lw	t2, _C_LABEL(cnt)+V_SWTCH	# for statistics
	lw	t1, _C_LABEL(whichqs)		# look for non-empty queue
	sw	s0, UADDR+U_PCB_CONTEXT+0	# do a 'savectx()'
	sw	s1, UADDR+U_PCB_CONTEXT+4
	sw	s2, UADDR+U_PCB_CONTEXT+8
	sw	s3, UADDR+U_PCB_CONTEXT+12
	mfc0	t0, MIPS_COP_0_STATUS_REG	# t0 = saved status register
	sw	s4, UADDR+U_PCB_CONTEXT+16
	sw	s5, UADDR+U_PCB_CONTEXT+20
	sw	s6, UADDR+U_PCB_CONTEXT+24
	sw	s7, UADDR+U_PCB_CONTEXT+28
	sw	s8, UADDR+U_PCB_CONTEXT+36
	sw	ra, UADDR+U_PCB_CONTEXT+40	# save return address
	sw	t0, UADDR+U_PCB_CONTEXT+44	# save status register
	addu	t2, t2, 1
	sw	t2, _C_LABEL(cnt)+V_SWTCH
	beq	t1, zero, _C_LABEL(idle)	# if none, idle
	mtc0	zero, MIPS_COP_0_STATUS_REG	# Disable all interrupts
sw1:
	nop					# wait for intrs disabled
	nop
	nop					# extra cycles on r4000	
	nop					# extra cycles on r4000	

	lw	t0, _C_LABEL(whichqs)		# look for non-empty queue
	li	t2, -1				# t2 = lowest bit set
	beq	t0, zero, _C_LABEL(idle)	# if none, idle
	move	t3, t0				# t3 = saved whichqs
1:
	addu	t2, t2, 1
	and	t1, t0, 1			# bit set?
	beq	t1, zero, 1b
	srl	t0, t0, 1			# try next bit
/*
 * Remove process from queue.
 */
	sll	t0, t2, 3
	la	t1, _C_LABEL(qs)
	addu	t0, t0, t1			# t0 = qp = &qs[highbit]
	lw	a0, P_FORW(t0)			# a0 = p = highest pri process
	nop
	lw	v0, P_FORW(a0)			# v0 = p->p_forw
	bne	t0, a0, 2f			# make sure something in queue
	sw	v0, P_FORW(t0)			# qp->ph_link = p->p_forw;
	PANIC("cpu_switch")			# nothing in queue
2:
	sw	t0, P_BACK(v0)			# p->p_forw->p_back = qp
	bne	v0, t0, 3f			# queue still not empty
	sw	zero, P_BACK(a0)		## for firewall checking
	li	v1, 1				# compute bit in 'whichqs'
	sll	v1, v1, t2
	xor	t3, t3, v1			# clear bit in 'whichqs'
	sw	t3, _C_LABEL(whichqs)
3:
/*
 * Switch to new context.
 */
	sw	zero, _C_LABEL(want_resched)
	jal	_C_LABEL(pmap_alloc_tlbpid)	# v0 = TLB PID
	move	s0, a0				# BDSLOT: save p
	sw	s0, _C_LABEL(curproc)		# set curproc
	sll	v0, v0, MIPS_TLB_PID_SHIFT	# v0 = aligned PID
	lw	t0, P_UPTE+0(s0)		# t0 = first u. pte
	lw	t1, P_UPTE+4(s0)		# t1 = 2nd u. pte
	or	v0, v0, UADDR			# v0 = first HI entry
/*
 * Resume process indicated by the pte's for its u struct
 * NOTE: This is hard coded to UPAGES == 2.
 * Also, there should be no TLB faults at this point.
 */
	mtc0	zero, MIPS_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MIPS_COP_0_TLB_HI		# init high entry
	mtc0	t0, MIPS_COP_0_TLB_LOW		# init low entry
	li	t0, 1 << MIPS1_TLB_INDEX_SHIFT
	tlbwi					# Write the TLB entry.
	addu	v0, v0, NBPG			# 2nd HI entry
	mtc0	t0, MIPS_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MIPS_COP_0_TLB_HI		# init high entry
	mtc0	t1, MIPS_COP_0_TLB_LOW		# init low entry
	nop
	tlbwi					# Write the TLB entry.
/*
 * Now running on new u struct.
 * Restore registers and return.
 */
	lw	v0, UADDR+U_PCB_CONTEXT+44	# restore kernel context
	lw	ra, UADDR+U_PCB_CONTEXT+40
	lw	s0, UADDR+U_PCB_CONTEXT+0
	lw	s1, UADDR+U_PCB_CONTEXT+4
	lw	s2, UADDR+U_PCB_CONTEXT+8
	lw	s3, UADDR+U_PCB_CONTEXT+12
	lw	s4, UADDR+U_PCB_CONTEXT+16
	lw	s5, UADDR+U_PCB_CONTEXT+20
	lw	s6, UADDR+U_PCB_CONTEXT+24
	lw	s7, UADDR+U_PCB_CONTEXT+28
	lw	sp, UADDR+U_PCB_CONTEXT+32
	lw	s8, UADDR+U_PCB_CONTEXT+36
	mtc0	v0, MIPS_COP_0_STATUS_REG
	j	ra
	li	v0, 1				# possible return to 'savectx()'
END(cpu_switch)

/*
 * {fu,su},{ibyte,isword,iword}, fetch or store a byte, short or word to
 * user text space.
 * {fu,su},{byte,sword,word}, fetch or store a byte, short or word to
 * user data space.
 */
LEAF(fuword)
ALEAF(fuiword)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	v0, 0(a0)			# fetch word
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fuword)

LEAF(fusword)
ALEAF(fuisword)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lhu	v0, 0(a0)			# fetch short
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fusword)

LEAF(fubyte)
ALEAF(fuibyte)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lbu	v0, 0(a0)			# fetch byte
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fubyte)

LEAF(suword)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sw	a1, 0(a0)			# store word
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(suword)

/*
 * Have to flush instruction cache afterwards.
 */
LEAF(suiword)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sw	a1, 0(a0)			# store word
	sw	zero, UADDR+U_PCB_ONFAULT
	move	v0, zero
/* XXXX FIXME */
#ifdef JONATHAN_BOTCHED_THIS
 	b	_C_LABEL(MachFlushICache)	# NOTE: must not clobber v0!
#else
/*XXX*/	b	_C_LABEL(mips1_FlushICache)# NOTE: must not clobber v0!
#endif
	li	a1, 4				# size of word
END(suiword)

/*
 * Will have to flush the instruction cache if byte merging is done in hardware.
 */
LEAF(susword)
ALEAF(suisword)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sh	a1, 0(a0)			# store short
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(susword)

LEAF(subyte)
ALEAF(suibyte)
	blt	a0, zero, _C_LABEL(fswberr)	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sb	a1, 0(a0)			# store byte
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(subyte)

LEAF(fswberr)
	j	ra
	li	v0, -1
END(fswberr)

/*
 * fuswintr and suswintr are just like fusword and susword except that if
 * the page is not in memory or would cause a trap, then we return an error.
 * The important thing is to prevent sleep() and switch().
 */
LEAF(fuswintr)
	blt	a0, zero, _C_LABEL(fswintrberr)	# make sure address is in user space
	li	v0, FSWINTRBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lhu	v0, 0(a0)			# fetch short
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fuswintr)

LEAF(suswintr)
	blt	a0, zero, _C_LABEL(fswintrberr)	# make sure address is in user space
	li	v0, FSWINTRBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sh	a1, 0(a0)			# store short
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(suswintr)

LEAF(fswintrberr)
	j	ra
	li	v0, -1
END(fswintrberr)

/*
 * Insert 'p' after 'q'.
 *	_insque(p, q)
 *		caddr_t p, q;
 */
LEAF(_insque)
	lw	v0, 0(a1)		# v0 = q->next
	sw	a1, 4(a0)		# p->prev = q
	sw	v0, 0(a0)		# p->next = q->next
	sw	a0, 4(v0)		# q->next->prev = p
	j	ra
	sw	a0, 0(a1)		# q->next = p
END(_insque)

/*
 * Remove item 'p' from queue.
 *	_remque(p)
 *		caddr_t p;
 */
LEAF(_remque)
	lw	v0, 0(a0)		# v0 = p->next
	lw	v1, 4(a0)		# v1 = p->prev
	nop
	sw	v0, 0(v1)		# p->prev->next = p->next
	j	ra
	sw	v1, 4(v0)		# p->next->prev = p->prev
END(_remque)



/* XXX 
 * XXX 
 * second-level (after locore vectors)  jump tables
 * were here */


#if 1	/* R2000/r3000 support, default on pmaxes */
# include "locore_r2000.S"
#endif	/* R2000/r3000 support, default on pmaxes */

#ifdef notyet	/* R4000 support  */
# include "locore_r4000.S"
#endif		/* R4000 support */


/*
 * Set/clear software interrupt routines.
 */

LEAF(setsoftclock)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# save status register
	mtc0	zero, MIPS_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	mfc0	v0, MIPS_COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, MIPS_SOFT_INT_MASK_0	# set soft clock interrupt
	mtc0	v0, MIPS_COP_0_CAUSE_REG	# save it
	mtc0	v1, MIPS_COP_0_STATUS_REG
	j	ra
	nop
END(setsoftclock)

LEAF(clearsoftclock)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# save status register
	mtc0	zero, MIPS_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MIPS_COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~MIPS_SOFT_INT_MASK_0	# clear soft clock interrupt
	mtc0	v0, MIPS_COP_0_CAUSE_REG	# save it
	mtc0	v1, MIPS_COP_0_STATUS_REG
	j	ra
	nop
END(clearsoftclock)

LEAF(setsoftnet)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# save status register
	mtc0	zero, MIPS_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MIPS_COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, MIPS_SOFT_INT_MASK_1	# set soft net interrupt
	mtc0	v0, MIPS_COP_0_CAUSE_REG	# save it
	mtc0	v1, MIPS_COP_0_STATUS_REG
	j	ra
	nop
END(setsoftnet)

LEAF(clearsoftnet)
	mfc0	v1, MIPS_COP_0_STATUS_REG	# save status register
	mtc0	zero, MIPS_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MIPS_COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~MIPS_SOFT_INT_MASK_1	# clear soft net interrupt
	mtc0	v0, MIPS_COP_0_CAUSE_REG	# save it
	mtc0	v1, MIPS_COP_0_STATUS_REG
	j	ra
	nop
END(clearsoftnet)

/*
 * Set/change interrupt priority routines.
 */

LEAF(MachEnableIntr)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	nop
	or	v0, v0, MIPS_SR_INT_IE
	mtc0	v0, MIPS_COP_0_STATUS_REG	# enable all interrupts
	j	ra
	nop
END(MachEnableIntr)

LEAF(spl0)
ALEAF(spllow)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	nop
	or	t0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
	mtc0	t0, MIPS_COP_0_STATUS_REG	# enable all interrupts
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(spl0)

LEAF(splsoftclock)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~MIPS_SOFT_INT_MASK_0	# disable soft clock
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(splsoftclock)

LEAF(splsoftnet)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(splsoftnet)

/*
 * hardware-level spls for hardware where the device interrupt priorites
 * are ordered, and map onto mips interrupt pins in increasing priority.
 * This maps directly onto BSD spl levels.
 */

/*
 * Block out int2 (hardware interrupt 0) and lower mips levels.
 */
LEAF(cpu_spl0)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl0)

/*
 * Block out Int3 (hardware interrupt 1) and lower mips levels.
 */
LEAF(cpu_spl1)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL1)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl1)

LEAF(cpu_spl2)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL2)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl2)

LEAF(cpu_spl3)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL3)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl3)

LEAF(cpu_spl4)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL4)

	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl4)

LEAF(cpu_spl5)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_SPL5)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(cpu_spl5)

/*
 * hardware-level spls for hardware where teh interrupt priorites
 * DO NOT  map onto levels.  
 *
 * For now, that means DEcstations that use only two distinct CPU
 * levels, one for TOD clock interrupts, and a second for all other
 * external devices (via an external controller.
 * XXX  the spl handling really needs re-writing from scratch.
 */
LEAF(Mach_spl0)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_0|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl0)

LEAF(Mach_spl1)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_1|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl1)

LEAF(Mach_spl2)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_2|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl2)

LEAF(Mach_spl3)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_3|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl3)

LEAF(Mach_spl4)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_4|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl4)

LEAF(Mach_spl5)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~(MIPS_INT_MASK_5|MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl5)


/*
 * We define an alternate entry point after mcount is called so it
 * can be used in mcount without causeing a recursive loop.
 */
LEAF(splhigh)
ALEAF(_splhigh)
	mfc0	v0, MIPS_COP_0_STATUS_REG	# read status register
	li	t0, ~MIPS_SR_INT_IE	# disable all interrupts
	and	t0, t0, v0
	mtc0	t0, MIPS_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MIPS_INT_MASK | MIPS_SR_INT_IE)
END(splhigh)

/*
 * Restore saved interrupt mask.
 */
LEAF(splx)
ALEAF(_splx)
	mfc0	v0, MIPS_COP_0_STATUS_REG
	li	t0, ~(MIPS_INT_MASK | MIPS_SR_INT_IE)
	and	t0, t0, v0
	or	t0, t0, a0
	mtc0	t0, MIPS_COP_0_STATUS_REG
	nop					# 3 ins to disable
	j	ra
	nop
END(splx)

/*----------------------------------------------------------------------------
 *
 * wbflush --
 *
 *	Return when the write buffer is empty.
 *
 *	wbflush()
 *	MachEmptyWriteBuffer()	[[backwards compatibility]]
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
LEAF(wbflush)
ALEAF(MachEmptyWriteBuffer)
	nop
	nop
	nop
	nop
1:	bc0f	1b
	nop
	j	ra
	nop
END(MachEmptyWriteBuffer)




/*----------------------------------------------------------------------------
 *
 * MachSwitchFPState --
 *
 *	Return the current value of the cause register.
 *
 *	MachGetCauseReg(void)
 *
 * Results:
 *	current value of Cause register.None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
LEAF(MachGetCauseReg)
	mfc0	v0, MIPS_COP_0_CAUSE_REG
	j	ra
	nop
END(MachGetCauseReg)

/*----------------------------------------------------------------------------
 *
 * MachSwitchFPState --
 *
 *	Save the current state into 'from' and restore it from 'to'.
 *
 *	MachSwitchFPState(from, to)
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
LEAF(MachSwitchFPState)
	mfc0	t1, MIPS_COP_0_STATUS_REG	# Save old SR
	li	t0, MIPS_SR_COP_1_BIT		# enable the coprocessor
	mtc0	t0, MIPS_COP_0_STATUS_REG

	beq	a0, zero, 1f			# skip save if NULL pointer
	nop
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	a0, P_ADDR(a0)			# get pointer to pcb for proc
	cfc1	t0, MIPS_FPU_CSR		# stall til FP done
	cfc1	t0, MIPS_FPU_CSR		# now get status
	li	t3, ~MIPS_SR_COP_1_BIT
	lw	t2, U_PCB_REGS+(PS * 4)(a0)	# get CPU status register
	sw	t0, U_PCB_FPREGS+(32 * 4)(a0)	# save FP status
	and	t2, t2, t3			# clear COP_1 enable bit
	sw	t2, U_PCB_REGS+(PS * 4)(a0)	# save new status register
/*
 * Save the floating point registers.
 */
	swc1	$f0, U_PCB_FPREGS+(0 * 4)(a0)
	swc1	$f1, U_PCB_FPREGS+(1 * 4)(a0)
	swc1	$f2, U_PCB_FPREGS+(2 * 4)(a0)
	swc1	$f3, U_PCB_FPREGS+(3 * 4)(a0)
	swc1	$f4, U_PCB_FPREGS+(4 * 4)(a0)
	swc1	$f5, U_PCB_FPREGS+(5 * 4)(a0)
	swc1	$f6, U_PCB_FPREGS+(6 * 4)(a0)
	swc1	$f7, U_PCB_FPREGS+(7 * 4)(a0)
	swc1	$f8, U_PCB_FPREGS+(8 * 4)(a0)
	swc1	$f9, U_PCB_FPREGS+(9 * 4)(a0)
	swc1	$f10, U_PCB_FPREGS+(10 * 4)(a0)
	swc1	$f11, U_PCB_FPREGS+(11 * 4)(a0)
	swc1	$f12, U_PCB_FPREGS+(12 * 4)(a0)
	swc1	$f13, U_PCB_FPREGS+(13 * 4)(a0)
	swc1	$f14, U_PCB_FPREGS+(14 * 4)(a0)
	swc1	$f15, U_PCB_FPREGS+(15 * 4)(a0)
	swc1	$f16, U_PCB_FPREGS+(16 * 4)(a0)
	swc1	$f17, U_PCB_FPREGS+(17 * 4)(a0)
	swc1	$f18, U_PCB_FPREGS+(18 * 4)(a0)
	swc1	$f19, U_PCB_FPREGS+(19 * 4)(a0)
	swc1	$f20, U_PCB_FPREGS+(20 * 4)(a0)
	swc1	$f21, U_PCB_FPREGS+(21 * 4)(a0)
	swc1	$f22, U_PCB_FPREGS+(22 * 4)(a0)
	swc1	$f23, U_PCB_FPREGS+(23 * 4)(a0)
	swc1	$f24, U_PCB_FPREGS+(24 * 4)(a0)
	swc1	$f25, U_PCB_FPREGS+(25 * 4)(a0)
	swc1	$f26, U_PCB_FPREGS+(26 * 4)(a0)
	swc1	$f27, U_PCB_FPREGS+(27 * 4)(a0)
	swc1	$f28, U_PCB_FPREGS+(28 * 4)(a0)
	swc1	$f29, U_PCB_FPREGS+(29 * 4)(a0)
	swc1	$f30, U_PCB_FPREGS+(30 * 4)(a0)
	swc1	$f31, U_PCB_FPREGS+(31 * 4)(a0)

1:
/*
 *  Restore the floating point registers.
 */
	lw	t0, U_PCB_FPREGS+(32 * 4)(a1)	# get status register
	lwc1	$f0, U_PCB_FPREGS+(0 * 4)(a1)
	lwc1	$f1, U_PCB_FPREGS+(1 * 4)(a1)
	lwc1	$f2, U_PCB_FPREGS+(2 * 4)(a1)
	lwc1	$f3, U_PCB_FPREGS+(3 * 4)(a1)
	lwc1	$f4, U_PCB_FPREGS+(4 * 4)(a1)
	lwc1	$f5, U_PCB_FPREGS+(5 * 4)(a1)
	lwc1	$f6, U_PCB_FPREGS+(6 * 4)(a1)
	lwc1	$f7, U_PCB_FPREGS+(7 * 4)(a1)
	lwc1	$f8, U_PCB_FPREGS+(8 * 4)(a1)
	lwc1	$f9, U_PCB_FPREGS+(9 * 4)(a1)
	lwc1	$f10, U_PCB_FPREGS+(10 * 4)(a1)
	lwc1	$f11, U_PCB_FPREGS+(11 * 4)(a1)
	lwc1	$f12, U_PCB_FPREGS+(12 * 4)(a1)
	lwc1	$f13, U_PCB_FPREGS+(13 * 4)(a1)
	lwc1	$f14, U_PCB_FPREGS+(14 * 4)(a1)
	lwc1	$f15, U_PCB_FPREGS+(15 * 4)(a1)
	lwc1	$f16, U_PCB_FPREGS+(16 * 4)(a1)
	lwc1	$f17, U_PCB_FPREGS+(17 * 4)(a1)
	lwc1	$f18, U_PCB_FPREGS+(18 * 4)(a1)
	lwc1	$f19, U_PCB_FPREGS+(19 * 4)(a1)
	lwc1	$f20, U_PCB_FPREGS+(20 * 4)(a1)
	lwc1	$f21, U_PCB_FPREGS+(21 * 4)(a1)
	lwc1	$f22, U_PCB_FPREGS+(22 * 4)(a1)
	lwc1	$f23, U_PCB_FPREGS+(23 * 4)(a1)
	lwc1	$f24, U_PCB_FPREGS+(24 * 4)(a1)
	lwc1	$f25, U_PCB_FPREGS+(25 * 4)(a1)
	lwc1	$f26, U_PCB_FPREGS+(26 * 4)(a1)
	lwc1	$f27, U_PCB_FPREGS+(27 * 4)(a1)
	lwc1	$f28, U_PCB_FPREGS+(28 * 4)(a1)
	lwc1	$f29, U_PCB_FPREGS+(29 * 4)(a1)
	lwc1	$f30, U_PCB_FPREGS+(30 * 4)(a1)
	lwc1	$f31, U_PCB_FPREGS+(31 * 4)(a1)

	and	t0, t0, ~MIPS_FPU_EXCEPTION_BITS
	ctc1	t0, MIPS_FPU_CSR
	nop

	mtc0	t1, MIPS_COP_0_STATUS_REG	# Restore the status register.
	j	ra
	nop
END(MachSwitchFPState)

/*----------------------------------------------------------------------------
 *
 * MachSaveCurFPState --
 *
 *	Save the current floating point coprocessor state.
 *
 *	MachSaveCurFPState(p)
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
LEAF(MachSaveCurFPState)
	lw	a0, P_ADDR(a0)			# get pointer to pcb for proc
	mfc0	t1, MIPS_COP_0_STATUS_REG	# Disable interrupts and
	li	t0, MIPS_SR_COP_1_BIT		#  enable the coprocessor
	mtc0	t0, MIPS_COP_0_STATUS_REG
	sw	zero, _C_LABEL(machFPCurProcPtr) # indicate state has been saved
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	t2, U_PCB_REGS+(PS * 4)(a0)	# get CPU status register
	li	t3, ~MIPS_SR_COP_1_BIT
	and	t2, t2, t3			# clear COP_1 enable bit
	cfc1	t0, MIPS_FPU_CSR		# stall til FP done
	cfc1	t0, MIPS_FPU_CSR		# now get status
	sw	t2, U_PCB_REGS+(PS * 4)(a0)	# save new status register
	sw	t0, U_PCB_FPREGS+(32 * 4)(a0)	# save FP status
/*
 * Save the floating point registers.
 */
	swc1	$f0, U_PCB_FPREGS+(0 * 4)(a0)
	swc1	$f1, U_PCB_FPREGS+(1 * 4)(a0)
	swc1	$f2, U_PCB_FPREGS+(2 * 4)(a0)
	swc1	$f3, U_PCB_FPREGS+(3 * 4)(a0)
	swc1	$f4, U_PCB_FPREGS+(4 * 4)(a0)
	swc1	$f5, U_PCB_FPREGS+(5 * 4)(a0)
	swc1	$f6, U_PCB_FPREGS+(6 * 4)(a0)
	swc1	$f7, U_PCB_FPREGS+(7 * 4)(a0)
	swc1	$f8, U_PCB_FPREGS+(8 * 4)(a0)
	swc1	$f9, U_PCB_FPREGS+(9 * 4)(a0)
	swc1	$f10, U_PCB_FPREGS+(10 * 4)(a0)
	swc1	$f11, U_PCB_FPREGS+(11 * 4)(a0)
	swc1	$f12, U_PCB_FPREGS+(12 * 4)(a0)
	swc1	$f13, U_PCB_FPREGS+(13 * 4)(a0)
	swc1	$f14, U_PCB_FPREGS+(14 * 4)(a0)
	swc1	$f15, U_PCB_FPREGS+(15 * 4)(a0)
	swc1	$f16, U_PCB_FPREGS+(16 * 4)(a0)
	swc1	$f17, U_PCB_FPREGS+(17 * 4)(a0)
	swc1	$f18, U_PCB_FPREGS+(18 * 4)(a0)
	swc1	$f19, U_PCB_FPREGS+(19 * 4)(a0)
	swc1	$f20, U_PCB_FPREGS+(20 * 4)(a0)
	swc1	$f21, U_PCB_FPREGS+(21 * 4)(a0)
	swc1	$f22, U_PCB_FPREGS+(22 * 4)(a0)
	swc1	$f23, U_PCB_FPREGS+(23 * 4)(a0)
	swc1	$f24, U_PCB_FPREGS+(24 * 4)(a0)
	swc1	$f25, U_PCB_FPREGS+(25 * 4)(a0)
	swc1	$f26, U_PCB_FPREGS+(26 * 4)(a0)
	swc1	$f27, U_PCB_FPREGS+(27 * 4)(a0)
	swc1	$f28, U_PCB_FPREGS+(28 * 4)(a0)
	swc1	$f29, U_PCB_FPREGS+(29 * 4)(a0)
	swc1	$f30, U_PCB_FPREGS+(30 * 4)(a0)
	swc1	$f31, U_PCB_FPREGS+(31 * 4)(a0)

	mtc0	t1, MIPS_COP_0_STATUS_REG	# Restore the status register.
	j	ra
	nop
END(MachSaveCurFPState)

/*----------------------------------------------------------------------------
 *
 * MachFPInterrupt --
 * MachFPTrap --
 *
 *	Handle a floating point interrupt (r3k) or trap (r4k).
 *	the handlers  are indentical, only the reporting mechanisms differ.
 *
 *	MachFPInterrupt(statusReg, causeReg, pc)
 *		unsigned statusReg;
 *		unsigned causeReg;
 *		unsigned pc;
 *
 *	MachFPTrap(statusReg, causeReg, pc)
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
NON_LEAF(MachFPInterrupt, STAND_FRAME_SIZE, ra)
	# XXX should use ANONLEAF (or ANESTED) instead of ALEAF.
ALEAF(MachFPTrap)

	subu	sp, sp, STAND_FRAME_SIZE
	mfc0	t0, MIPS_COP_0_STATUS_REG
	sw	ra, STAND_RA_OFFSET(sp)
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)

	or	t1, t0, MIPS_SR_COP_1_BIT
	mtc0	t1, MIPS_COP_0_STATUS_REG
	nop
	nop
	nop				# 1st extra nop for r4k
	nop				# 2nd extra nop for r4k

	cfc1	t1, MIPS_FPU_CSR	# stall til FP done
	cfc1	t1, MIPS_FPU_CSR	# now get status
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
	sw	a2, STAND_FRAME_SIZE + 8(sp)
	li	a0, UADDR+U_PCB_REGS		# first arg is ptr to CPU registers
	move	a1, a2				# second arg is instruction PC
	move	a2, t1				# third arg is floating point CSR
	jal	_C_LABEL(MachEmulateBranch)	# compute PC after branch
	move	a3, zero			# fourth arg is FALSE
/*
 * Now load the floating-point instruction in the branch delay slot
 * to be emulated.
 */
	lw	a2, STAND_FRAME_SIZE + 8(sp)	# restore EXC pc
	b	2f
	lw	a0, 4(a2)			# a0 = coproc instruction
/*
 * This is not in the branch delay slot so calculate the resulting
 * PC (epc + 4) into v0 and continue to MachEmulateFP().
 */
1:
	lw	a0, 0(a2)			# a0 = coproc instruction
	addu	v0, a2, 4			# v0 = next pc
2:
	sw	v0, UADDR+U_PCB_REGS+(PC * 4)	# save new pc
/*
 * Check to see if the instruction to be emulated is a floating-point
 * instruction.
 */
	srl	a3, a0, MIPS_OPCODE_SHIFT
	beq	a3, MIPS_OPCODE_C1, 4f		# this should never fail
	nop
/*
 * Send a floating point exception signal to the current process.
 */
3:
	lw	a0, _C_LABEL(curproc)		# get current process
	cfc1	a2, MIPS_FPU_CSR		# code = FP execptions
	ctc1	zero, MIPS_FPU_CSR		# Clear exceptions
	jal	_C_LABEL(trapsignal)
	li	a1, SIGFPE
	b	FPReturn
	nop

/*
 * Finally, we can call MachEmulateFP() where a0 is the instruction to emulate.
 */
4:
	jal	_C_LABEL(MachEmulateFP)
	nop

/*
 * Turn off the floating point coprocessor and return.
 */
FPReturn:
	mfc0	t0, MIPS_COP_0_STATUS_REG
	lw	ra, STAND_RA_OFFSET(sp)
	and	t0, t0, ~MIPS_SR_COP_1_BIT
	mtc0	t0, MIPS_COP_0_STATUS_REG
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(MachFPInterrupt)


#ifdef KADB
/*
 * Read a long and return it.
 * Note: addresses can be unaligned!
 *
 * long
L* kdbpeek(addr)
L*	caddt_t addr;
L* {
L*	return (*(long *)addr);
L* }
 */
LEAF(kdbpeek)
ALEAF(mdbpeek)
	li	v0, KADBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	and	v0, a0, 3		# unaligned address?
	bne	v0, zero, 1f
	nop
	b	2f
	lw	v0, (a0)		# aligned access
1:
	lwr	v0, 0(a0)		# get next 4 bytes (unaligned)
	lwl	v0, 3(a0)
2:
	j	ra			# made it w/o errors
	sw	zero, UADDR+U_PCB_ONFAULT
kadberr:
	li	v0, 1			# trap sends us here
	sw	v0, kdbmkfault
	j	ra
	nop
END(kdbpeek)

/*
 * Write a long to 'addr'.
 * Note: addresses can be unaligned!
 *
L* void
L* kdbpoke(addr, value)
L*	caddt_t addr;
L*	long value;
L* {
L*	*(long *)addr = value;
L* }
 */
LEAF(kdbpoke)
ALEAF(mdbpoke)
	li	v0, KADBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	and	v0, a0, 3		# unaligned address?
	bne	v0, zero, 1f
	nop
	b	2f
	sw	a1, (a0)		# aligned access
1:
	swr	a1, 0(a0)		# store next 4 bytes (unaligned)
	swl	a1, 3(a0)
	and	a0, a0, ~3		# align address for cache flush
2:
	sw	zero, UADDR+U_PCB_ONFAULT
	b	_C_LABEL(MachFlushICache)	# flush instruction cache
	li	a1, 8
END(kdbpoke)

/*
 * Save registers and state so we can do a 'kdbreset' (like longjmp) later.
 * Always returns zero.
 *
L* int kdb_savearea[11];
L*
L* int
L* kdbsetexit()
L* {
L*	kdb_savearea[0] = 0;
L*	return (0);
L* }
 */
	.comm	kdb_savearea, (11 * 4)

LEAF(kdbsetexit)
ALEAF(mdbsetexit)
	la	a0, kdb_savearea
	sw	s0, 0(a0)
	sw	s1, 4(a0)
	sw	s2, 8(a0)
	sw	s3, 12(a0)
	sw	s4, 16(a0)
	sw	s5, 20(a0)
	sw	s6, 24(a0)
	sw	s7, 28(a0)
	sw	sp, 32(a0)
	sw	s8, 36(a0)
	sw	ra, 40(a0)
	j	ra
	move	v0, zero
END(kdbsetexit)

/*
 * Restore registers and state (like longjmp) and return x.
 *
L* int
L* kdbreset(x)
L* {
L*	return (x);
L* }
 */
LEAF(kdbreset)
ALEAF(mdbreset)
	la	v0, kdb_savearea
	lw	ra, 40(v0)
	lw	s0, 0(v0)
	lw	s1, 4(v0)
	lw	s2, 8(v0)
	lw	s3, 12(v0)
	lw	s4, 16(v0)
	lw	s5, 20(v0)
	lw	s6, 24(v0)
	lw	s7, 28(v0)
	lw	sp, 32(v0)
	lw	s8, 36(v0)
	j	ra
	move	v0, a0
END(kdbreset)

/*
 * Trap into the debugger.
 *
L* void
L* kdbpanic()
L* {
L* }
 */
LEAF(kdbpanic)
ALEAF(mdbpanic)
	break	MIPS_BREAK_KDB_VAL
	j	ra
	nop
END(kdbpanic)
#endif /* KADB */

LEAF(cpu_getregs)
	sw	sp, 0(a0)
	sw	ra, 4(a0)
	j	ra
	sw	s8, 8(a0)
END(cpu_getregs)


/*
 * Port-specific locore code moved to sys/arch/<port>/<port>/locore_machdep.S
 */
