/*	$OpenBSD: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $	*/
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
 *	from: @(#)locore.s	8.5 (Berkeley) 1/4/94
 *	$Id: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $
 */

/*
 *	Contains code that is the first executed at boot time plus
 *	assembly language support routines.
 */

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/pte.h>

#include "assym.h"

	.set	noreorder		# Noreorder is default style!

/*
 * Amount to take off of the stack for the benefit of the debugger.
 */
#define START_FRAME	((4 * 4) + 4 + 4)

	.globl	start
	.globl	kernel_text
kernel_text = start
start:
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	mtc0	zero, COP_0_CAUSE_REG	# Clear soft interrupts

/*
 * Initialize stack and call machine startup.
 */
	la	sp, start - START_FRAME
	la	gp, _gp
	sw	zero, START_FRAME - 4(sp)	# Zero out old ra for debugger
	jal	mips_init			# mips_init(argc, argv, envp)
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger

	li	t0, SR_COP_1_BIT		# Disable interrupts and
	mtc0	t0, COP_0_STATUS_REG		#   enable the fp coprocessor
	li	sp, KERNELSTACK - START_FRAME	# switch to standard stack
	nop
	nop					# wait for new status to
	nop					# wait for new status to
	nop					# to be effective
	nop
	cfc1	t1, FPC_ID			# read FPU ID register
	sw	t1, fpu_id			# save FPU ID register
	jal	main				# main(regs)
	move	a0, zero
	PANIC("Startup failed!")
/*
 * Setup for and return to user.
 */
LEAF(proc_trampoline)
	jal	s0
	move	a0,s1			# set up for return to user.

	.set	noat
	li	v0, SR_EXL
	mtc0	v0, COP_0_STATUS_REG	# set exeption level bit.
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtlo	t0
	mthi	t1
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, COP_0_EXC_PC	# set return address
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	mtc0	a0, COP_0_STATUS_REG	# switch to user mode (when eret...)
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
	lw	gp, UADDR+U_PCB_REGS+(GP * 4)
	lw	sp, UADDR+U_PCB_REGS+(SP * 4)
	lw	s8, UADDR+U_PCB_REGS+(S8 * 4)
	lw	ra, UADDR+U_PCB_REGS+(RA * 4)
	lw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	eret
	.set	at
END(proc_trampoline)

/*
 * Primitives
 */

/*
 * This table is indexed by u.u_pcb.pcb_onfault in trap().
 * The reason for using this table rather than storing an address in
 * u.u_pcb.pcb_onfault is simply to make the code faster.
 */
	.globl	onfault_table
	.data
	.align	3
onfault_table:
	.word	0		# invalid index number
#define BADERR		1
	.word	baderr
#define COPYERR		2
	.word	copyerr
#define FSWBERR		3
	.word	fswberr
#define FSWINTRBERR	4
	.word	fswintrberr
#if defined(DDB) || defined(DEBUG)
#define	DDBERR		5
	.word	ddberr
#else
	.word	0
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
 * This code is copied the user's stack for returning from signal handlers
 * (see sendsig() and sigreturn()). We have to compute the address
 * of the sigcontext struct for the sigreturn call.
 */
	.globl	sigcode
sigcode:
	addu	a0, sp, 16		# address of sigcontext
	li	v0, SYS_sigreturn	# sigreturn(scp)
	syscall
	break	0			# just in case sigreturn fails
	.globl	esigcode
esigcode:

/*
 * Copy a null terminated string within the kernel address space.
 * Maxlength may be null if count not wanted.
 *	copystr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
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
 * Optimized memory zero code.
 * mem_zero_page(addr);
 */
LEAF(mem_zero_page)
	li	v0, NBPG
1:
	subu	v0, 8
	sd	zero, 0(a0)
	bne	zero, v0, 1b
	addu	a0, 8
	jr	ra
	nop
END(mem_zero_page)

/*
 *	Block I/O routines mainly used by I/O drivers.
 *
 *	Args as:	a0 = port
 *			a1 = memory address
 *			a2 = count
 */
LEAF(insb)
	beq	a2, zero, 2f
	addu	a2, a1
1:
	lbu	v0, 0(a0)
	addiu	a1, 1
	bne	a1, a2, 1b
	sb	v0, -1(a1)
2:
	jr	ra
	nop
END(insb)

LEAF(insw)
	beq	a2, zero, 2f
	addu	a2, a2
	addu	a2, a1
1:
	lhu	v0, 0(a0)
	addiu	a1, 2
	bne	a1, a2, 1b
	sh	v0, -2(a1)
2:
	jr	ra
	nop
END(insw)

LEAF(insl)
	beq	a2, zero, 2f
	sll	a2, 2
	addu	a2, a1
1:
	lw	v0, 0(a0)
	addiu	a1, 4
	bne	a1, a2, 1b
	sw	v0, -4(a1)
2:
	jr	ra
	nop
END(insl)

LEAF(outsb)
	beq	a2, zero, 2f
	addu	a2, a1
1:
	lbu	v0, 0(a1)
	addiu	a1, 1
	bne	a1, a2, 1b
	sb	v0, 0(a0)
2:
	jr	ra
	nop
END(outsb)

LEAF(outsw)
	beq	a2, zero, 2f
	addu	a2, a2
	li	v0, 1
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	addu	a2, a1
1:
	lhu	v0, 0(a1)
	addiu	a1, 2
	bne	a1, a2, 1b
	sh	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	addiu	a1, 2
	bne	a1, a2, 3b
	sh	v0, 0(a0)

	jr	ra
	nop
END(outsw)

LEAF(outsl)
	beq	a2, zero, 2f
	sll	a2, 2
	li	v0, 3
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	addu	a2, a1
1:
	lw	v0, 0(a1)
	addiu	a1, 4
	bne	a1, a2, 1b
	sw	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	addiu	a1, 4
	bne	a1, a2, 3b
	sw	v0, 0(a0)

	jr	ra
	nop
END(outsl)

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 *
 *	copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyinstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a0, zero, copyerr	# make sure address is in user space
	li	v0, COPYERR
	jal	copystr
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
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyoutstr, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	sw	ra, STAND_RA_OFFSET(sp)
	blt	a1, zero, copyerr	# make sure address is in user space
	li	v0, COPYERR
	jal	copystr
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
	blt	a0, zero, copyerr	# make sure address is in user space
	li	v0, COPYERR
	jal	bcopy
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
	blt	a1, zero, copyerr	# make sure address is in user space
	li	v0, COPYERR
	jal	bcopy
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
	mfc0	v0, COP_0_STATUS_REG
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
 * The following primitives manipulate the run queues.  _whichqs tells which
 * of the 32 queues _qs have processes in them.  Setrunqueue puts processes
 * into queues, Remrq removes them from queues.  The running process is on
 * no queue, other processes are on a queue related to p->p_priority, divided
 * by 4 actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
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
	lw	t2, whichqs		# set corresponding bit
	nop
	or	t2, t2, t1
	sw	t2, whichqs
	sll	t0, t0, 3		# compute index into 'qs'
	la	t1, qs
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
 * Remrq(p)
 *
 * Call should be made at splclock().
 */
NON_LEAF(remrunqueue, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	lbu	t0, P_PRIORITY(a0)	# get from p->p_priority / 4 queue
	li	t1, 1			# compute corresponding bit
	srl	t0, t0, 2		# compute index into 'whichqs'
	lw	t2, whichqs		# check corresponding bit
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
	la	v0, qs
	addu	t0, t0, v0		# t0 = qp = &qs[pri >> 2]
	lw	v0, P_FORW(t0)		# check if queue empty
	nop
	bne	v0, t0, 2f		# No. qp->ph_link != qp
	nop
	xor	t2, t2, t1		# clear corresponding bit in 'whichqs'
	sw	t2, whichqs
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
	la	v1, nullproc			# save state into garbage proc
	lw	t0, P_UPTE+0(v1)		# t0 = first u. pte
	lw	t1, P_UPTE+4(v1)		# t1 = 2nd u. pte
	li	v0, UADDR			# v0 = first HI entry
	mtc0	zero, COP_0_TLB_INDEX	# set the index register
	dmtc0	v0, COP_0_TLB_HI		# init high entry
	dmtc0	t0, COP_0_TLB_LO0		# init low entry0
	dmtc0	t1, COP_0_TLB_LO1		# init low entry1
	nop
	tlbwi					# Write the TLB entry.
	nop
	nop
	sw	zero, curproc
	b	cpu_switch
	li	sp, KERNELSTACK - START_FRAME	# switch to standard stack
END(switch_exit)

/*
 * When no processes are on the runq, cpu_switch branches to idle
 * to wait for something to come ready.
 * Note: this is really a part of cpu_switch() but defined here for kernel
 * profiling.
 */
LEAF(idle)
	li	t0, (INT_MASK | SR_INT_ENAB)
	mtc0	t0, COP_0_STATUS_REG	# enable all interrupts
	sw	zero, curproc			# set curproc NULL for stats
	li	t1,1
1:
	beq	t1, zero, 2f
	addu	t1, t1, 1
	lw	t0, whichqs			# look for non-empty queue
	nop
	beq	t0, zero, 1b
	nop
	b	sw1
	mtc0	zero, COP_0_STATUS_REG	# Disable all interrupts
2:
	break   BREAK_SOVER_VAL
	b	1b
	nop
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
	lw	t2, cnt+V_SWTCH			# for statistics
	lw	t1, whichqs			# look for non-empty queue
	sw	s0, UADDR+U_PCB_CONTEXT+0	# do a 'savectx()'
	sw	s1, UADDR+U_PCB_CONTEXT+4
	sw	s2, UADDR+U_PCB_CONTEXT+8
	sw	s3, UADDR+U_PCB_CONTEXT+12
	mfc0	t0, COP_0_STATUS_REG	# t0 = saved status register
	sw	s4, UADDR+U_PCB_CONTEXT+16
	sw	s5, UADDR+U_PCB_CONTEXT+20
	sw	s6, UADDR+U_PCB_CONTEXT+24
	sw	s7, UADDR+U_PCB_CONTEXT+28
	sw	s8, UADDR+U_PCB_CONTEXT+36
	sw	ra, UADDR+U_PCB_CONTEXT+40	# save return address
	sw	t0, UADDR+U_PCB_CONTEXT+44	# save status register
	addu	t2, t2, 1
	sw	t2, cnt+V_SWTCH
	beq	t1, zero, idle			# if none, idle
	mtc0	zero, COP_0_STATUS_REG	# Disable all interrupts
sw1:
	nop					# wait for intrs disabled
	nop
	nop
	nop
	lw	t0, whichqs			# look for non-empty queue
	li	t2, -1				# t2 = lowest bit set
	beq	t0, zero, idle			# if none, idle
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
	la	t1, qs
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
	sw	t3, whichqs
3:
/*
 * Switch to new context.
 */
	sw	zero, want_resched
	jal	pmap_alloc_tlbpid		# v0 = TLB PID
	move	s0, a0				# BDSLOT: save p
	sw	s0, curproc			# set curproc
	lw	t0, P_UPTE+0(s0)		# t0 = first u. pte
	lw	t1, P_UPTE+4(s0)		# t1 = 2nd u. pte
	or	v0, v0, UADDR			# v0 = first HI entry
/*
 * Resume process indicated by the pte's for its u struct
 * NOTE: This is hard coded to UPAGES == 2.
 * Also, there should be no TLB faults at this point.
 */
	mtc0	zero, COP_0_TLB_INDEX	# set the index register
	dmtc0	v0, COP_0_TLB_HI		# init high entry
	dmtc0	t0, COP_0_TLB_LO0		# init low entry0
	dmtc0	t1, COP_0_TLB_LO1		# init low entry1
	nop
	tlbwi					# Write the TLB entry.
	nop					# Delay for effect
	nop					# Delay for effect
	nop
	nop
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
	mtc0	v0, COP_0_STATUS_REG
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
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lw	v0, 0(a0)		# fetch word
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fuword)

LEAF(fusword)
ALEAF(fuisword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lhu	v0, 0(a0)		# fetch short
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fusword)

LEAF(fubyte)
ALEAF(fuibyte)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lbu	v0, 0(a0)		# fetch byte
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fubyte)

LEAF(suword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sw	a1, 0(a0)		# store word
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(suword)

/*
 * Have to flush instruction cache afterwards.
 */
LEAF(suiword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sw	a1, 0(a0)		# store word
	sw	zero, UADDR+U_PCB_ONFAULT
	b	R4K_FlushICache		# FlushICache sets v0 = 0. (Ugly)
	li	a1, 4			# size of word
END(suiword)

/*
 * Will have to flush the instruction cache if byte merging is done in hardware.
 */
LEAF(susword)
ALEAF(suisword)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sh	a1, 0(a0)		# store short
	sw	zero, UADDR+U_PCB_ONFAULT
	j	ra
	move	v0, zero
END(susword)

LEAF(subyte)
ALEAF(suibyte)
	blt	a0, zero, fswberr	# make sure address is in user space
	li	v0, FSWBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sb	a1, 0(a0)		# store byte
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
	blt	a0, zero, fswintrberr	# make sure address is in user space
	li	v0, FSWINTRBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	lhu	v0, 0(a0)		# fetch short
	j	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(fuswintr)

LEAF(suswintr)
	blt	a0, zero, fswintrberr	# make sure address is in user space
	li	v0, FSWINTRBERR
	sw	v0, UADDR+U_PCB_ONFAULT
	sh	a1, 0(a0)		# store short
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

/*
 * This code is copied to the TLB exception vector address to
 * handle TLB translation misses.
 * NOTE: This code must be relocatable and max 32 instructions!!!
 * Don't check for invalid pte's here. We load them as well and
 * let the processor trap to load the correct value after service.
 */
	.globl	MipsTLBMiss
MipsTLBMiss:
	.set	noat
	dmfc0	k0, COP_0_BAD_VADDR	# get the virtual address
	lw	k1, UADDR+U_PCB_SEGTAB		# get the current segment table
	bltz	k0, 1f				# kernel address space ->
	srl	k0, k0, SEGSHIFT - 2		# compute segment table index
	andi	k0, k0, 0x7fc			# PMAP_SEGTABSIZ-1
	addu	k1, k1, k0
	dmfc0	k0, COP_0_BAD_VADDR	# get the virtual address
	lw	k1, 0(k1)			# get pointer to segment map
	srl	k0, k0, PGSHIFT - 2		# compute segment map index
	andi	k0, k0, ((NPTEPG/2) - 1) << 3
	beq	k1, zero, 2f			# invalid segment map
	addu	k1, k1, k0			# index into segment map
	lw	k0, 0(k1)			# get page PTE
	lw	k1, 4(k1)
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, COP_0_TLB_LO1
	nop
	tlbwr					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
1:
	j	MipsTLBMissException
	nop
2:
	j	SlowFault
	nop

	.globl	MipsTLBMissEnd
MipsTLBMissEnd:
	.set	at

/*
 * This code is copied to the general exception vector address to
 * handle all execptions except RESET and TLBMiss.
 * NOTE: This code must be relocatable!!!
 */
	.globl	MipsException
MipsException:
/*
 * Find out what mode we came from and jump to the proper handler.
 */
	.set	noat
	mfc0	k0, COP_0_STATUS_REG	# Get the status register
	mfc0	k1, COP_0_CAUSE_REG	# Get the cause register value.
	and	k0, k0, SR_KSU_USER	# test for user mode
						# sneaky but the bits are
						# with us........
	sll	k0, k0, 3			# shift user bit for cause index
	and	k1, k1, CR_EXC_CODE	# Mask out the cause bits.
	or	k1, k1, k0			# change index to user table
1:
	la	k0, machExceptionTable		# get base of the jump table
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
	.globl	MipsExceptionEnd
MipsExceptionEnd:

/*
 * We couldn't find a TLB entry.
 * Find out what mode we came from and call the appropriate handler.
 */
SlowFault:
	.set	noat
	mfc0	k0, COP_0_STATUS_REG
	nop
	and	k0, k0, SR_KSU_USER
	bne	k0, zero, MipsUserGenException
	nop
	.set	at
/*
 * Fall though ...
 */

/*----------------------------------------------------------------------------
 *
 * MipsKernGenException --
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
#ifndef MIPS_ISAIII
#define	STORE	sw	/* 32 bit mode regsave instruction */
#define	LOAD	lw	/* 32 bit mode regload instruction */
#define	RSIZE	4	/* 32 bit mode register size */
#else
#define	STORE	sd	/* 64 bit mode regsave instruction */
#define	STORE	ld	/* 64 bit mode regload instruction */
#define	RSIZE	8	/* 64 bit mode register size */
#endif

#define	SAVE_REG(reg, offs, base) \
	STORE	reg, STAND_ARG_SIZE + (RSIZE * offs) (base)

#define	SAVE_CPU \
	SAVE_REG(AT, AST, sp)		;\
	SAVE_REG(v0, V0, sp)		;\
	SAVE_REG(v1, V1, sp)		;\
	SAVE_REG(a0, A0, sp)		;\
	SAVE_REG(a1, A1, sp)		;\
	SAVE_REG(a2, A2, sp)		;\
	SAVE_REG(a3, A3, sp)		;\
	SAVE_REG(t0, T0, sp)		;\
	SAVE_REG(t1, T1, sp)		;\
	SAVE_REG(t2, T2, sp)		;\
	SAVE_REG(t3, T3, sp)		;\
	SAVE_REG(t4, T4, sp)		;\
	SAVE_REG(t5, T5, sp)		;\
	SAVE_REG(t6, T6, sp)		;\
	SAVE_REG(t7, T7, sp)		;\
	SAVE_REG(t8, T8, sp)		;\
	SAVE_REG(t9, T9, sp)		;\
	mflo	v0			;\
	mfhi	v1			;\
	mfc0	a0, COP_0_STATUS_REG	;\
	mfc0	a1, COP_0_CAUSE_REG	;\
	mfc0	a2, COP_0_BAD_VADDR	;\
	mfc0	a3, COP_0_EXC_PC	;\
	SAVE_REG(v0, MULLO, sp)		;\
	SAVE_REG(v1, MULHI, sp)		;\
	SAVE_REG(a0, SR, sp)		;\
	SAVE_REG(a1, CAUSE, sp)		;\
	SAVE_REG(ra, RA, sp)		;\
	SAVE_REG(a2, BADVADDR, sp)	;\
	SAVE_REG(a3, PC, sp)		;\
	addu	v0, sp, KERN_EXC_FRAME_SIZE ;\
	SAVE_REG(v0, SP, sp)		;\
	addu	a0, sp, STAND_ARG_SIZE	;\
	mtc0	zero,COP_0_STATUS_REG

#ifdef DDB
#define	SAVE_CPU_DDB \
	SAVE_REG(s0, S0, sp)		;\
	SAVE_REG(s1, S1, sp)		;\
	SAVE_REG(s2, S2, sp)		;\
	SAVE_REG(s3, S3, sp)		;\
	SAVE_REG(s4, S4, sp)		;\
	SAVE_REG(s5, S5, sp)		;\
	SAVE_REG(s6, S6, sp)		;\
	SAVE_REG(s7, S7, sp)		;\
	SAVE_REG(s8, S8, sp)
#else
#define	SAVE_CPU_DDB
#endif

#define	RESTORE_REG(reg, offs, base) \
	LOAD	reg, STAND_ARG_SIZE + (RSIZE * offs) (base)

#define	RESTORE_CPU \
	mtc0	zero,COP_0_STATUS_REG	;\
	RESTORE_REG(a0, SR, sp)		;\
	RESTORE_REG(t0, MULLO, sp)	;\
	RESTORE_REG(t1, MULHI, sp)	;\
	mtc0	a0, COP_0_STATUS_REG	;\
	mtlo	t0			;\
	mthi	t1			;\
	dmtc0	v0, COP_0_EXC_PC	;\
	RESTORE_REG(AT, AST, sp)		;\
	RESTORE_REG(v0, V0, sp)		;\
	RESTORE_REG(v1, V1, sp)		;\
	RESTORE_REG(a0, A0, sp)		;\
	RESTORE_REG(a1, A1, sp)		;\
	RESTORE_REG(a2, A2, sp)		;\
	RESTORE_REG(a3, A3, sp)		;\
	RESTORE_REG(t0, T0, sp)		;\
	RESTORE_REG(t1, T1, sp)		;\
	RESTORE_REG(t2, T2, sp)		;\
	RESTORE_REG(t3, T3, sp)		;\
	RESTORE_REG(t4, T4, sp)		;\
	RESTORE_REG(t5, T5, sp)		;\
	RESTORE_REG(t6, T6, sp)		;\
	RESTORE_REG(t7, T7, sp)		;\
	RESTORE_REG(t8, T8, sp)		;\
	RESTORE_REG(t9, T9, sp)		;\
	RESTORE_REG(ra, RA, sp)		;\
	addu	sp, sp, KERN_EXC_FRAME_SIZE

/*
 * The kernel exception stack contains 18 saved general registers,
 * the status register and the multiply lo and high registers.
 * In addition, we set this up for linkage conventions.
 */
#define KERN_REG_SIZE		(NUMSAVEREGS * RSIZE)
#define	KERN_EXC_FRAME_SIZE	(STAND_FRAME_SIZE + KERN_REG_SIZE + 16)

NNON_LEAF(MipsKernGenException, KERN_EXC_FRAME_SIZE, ra)
	.set	noat
	subu	sp, sp, KERN_EXC_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - KERN_EXC_FRAME_SIZE)
/*
 *  Save CPU state, building 'frame'.
 */
	SAVE_CPU
	SAVE_CPU_DDB
/*
 *  Call the exception handler. a0 points at the saved frame.
 */
	jal	trap
	sw	a3, STAND_RA_OFFSET + KERN_REG_SIZE(sp)		# for debugging
/*
 * Restore registers and return from the exception.
 * v0 contains the return address.
 */
	RESTORE_CPU
	eret					#  exception.
	.set	at
END(MipsKernGenException)

/*----------------------------------------------------------------------------
 *
 * MipsUserGenException --
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
NNON_LEAF(MipsUserGenException, STAND_FRAME_SIZE, ra)
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
	mfc0	a0, COP_0_STATUS_REG	# First arg is the status reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	s0, UADDR+U_PCB_REGS+(S0 * 4)
	mfc0	a1, COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	s1, UADDR+U_PCB_REGS+(S1 * 4)
	sw	s2, UADDR+U_PCB_REGS+(S2 * 4)
	sw	s3, UADDR+U_PCB_REGS+(S3 * 4)
	sw	s4, UADDR+U_PCB_REGS+(S4 * 4)
	mfc0	a2, COP_0_BAD_VADDR	# Third arg is the fault addr
	sw	s5, UADDR+U_PCB_REGS+(S5 * 4)
	sw	s6, UADDR+U_PCB_REGS+(S6 * 4)
	sw	s7, UADDR+U_PCB_REGS+(S7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a3, COP_0_EXC_PC		# Fourth arg is the pc.
	sw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	sw	gp, UADDR+U_PCB_REGS+(GP * 4)
	sw	sp, UADDR+U_PCB_REGS+(SP * 4)
	sw	s8, UADDR+U_PCB_REGS+(S8 * 4)
	li	sp, KERNELSTACK - STAND_FRAME_SIZE	# switch to kernel SP
	sw	ra, UADDR+U_PCB_REGS+(RA * 4)
	sw	v0, UADDR+U_PCB_REGS+(MULLO * 4)
	sw	v1, UADDR+U_PCB_REGS+(MULHI * 4)
	sw	a0, UADDR+U_PCB_REGS+(SR * 4)
	sw	a1, UADDR+U_PCB_REGS+(CAUSE * 4)
	sw	a2, UADDR+U_PCB_REGS+(BADVADDR * 4)
	sw	a3, UADDR+U_PCB_REGS+(PC * 4)
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
	la	gp, _gp				# switch to kernel GP
	.set	at
# Turn off fpu and enter kernel mode
	and	t0, a0, ~(SR_COP_1_BIT | SR_EXL | SR_KSU_MASK | SR_INT_ENAB)
	.set	noat
	li	a0, UADDR+U_PCB_REGS
/*
 * Call the exception handler.
 */
	jal	trap
	mtc0	t0, COP_0_STATUS_REG
/*
 * Restore user registers and return.
 * First disable interrupts and set exeption level.
 */
	mtc0	zero, COP_0_STATUS_REG	# disable int
	nop
	nop
	nop
	li	v0, SR_EXL
	mtc0	v0, COP_0_STATUS_REG	# set exeption level

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, COP_0_STATUS_REG	# still exeption level
	mtlo	t0
	mthi	t1
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, COP_0_EXC_PC		# set return address
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
	eret
	.set	at
END(MipsUserGenException)

/*----------------------------------------------------------------------------
 *
 * MipsKernIntr --
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

NNON_LEAF(MipsKernIntr, KERN_EXC_FRAME_SIZE, ra)
	.set	noat
	subu	sp, sp, KERN_EXC_FRAME_SIZE
	.mask	0x80000000, (STAND_RA_OFFSET - KERN_EXC_FRAME_SIZE)
/*
 * Save the relevant kernel registers onto the stack.
 * We don't need to save s0 - s8, sp and gp because
 * the compiler does it for us.
 */
	SAVE_CPU
/*
 * Call the interrupt handler.
 */
	jal	interrupt
	sw	a3, STAND_RA_OFFSET + KERN_REG_SIZE(sp)
/*
 * Restore registers and return from the interrupt.
 */
	lw	v0, STAND_RA_OFFSET + KERN_REG_SIZE(sp)
	RESTORE_CPU
	eret
	.set	at
END(MipsKernIntr)

/*----------------------------------------------------------------------------
 *
 * MipsUserIntr --
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
NNON_LEAF(MipsUserIntr, STAND_FRAME_SIZE, ra)
	.set	noat
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
/*
 * Save the relevant user registers into the u.u_pcb struct.
 * We don't need to save s0 - s8 because the compiler does it for us.
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
	mfc0	a0, COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	sw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	sw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	sw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	mfc0	a1, COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a3, COP_0_EXC_PC		# Fourth arg is the pc.
	sw	t9, UADDR+U_PCB_REGS+(T9 * 4)
	sw	gp, UADDR+U_PCB_REGS+(GP * 4)
	sw	sp, UADDR+U_PCB_REGS+(SP * 4)
	sw	ra, UADDR+U_PCB_REGS+(RA * 4)
	li	sp, KERNELSTACK - STAND_FRAME_SIZE	# switch to kernel SP
	sw	v0, UADDR+U_PCB_REGS+(MULLO * 4)
	sw	v1, UADDR+U_PCB_REGS+(MULHI * 4)
	sw	a0, UADDR+U_PCB_REGS+(SR * 4)
	sw	a1, UADDR+U_PCB_REGS+(CAUSE * 4)
	sw	a3, UADDR+U_PCB_REGS+(PC * 4)
	la	gp, _gp				# switch to kernel GP
# Turn off fpu and enter kernel mode
	.set	at
	and	t0, a0, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	.set	noat
	mtc0	t0, COP_0_STATUS_REG
	li	a0, UADDR+U_PCB_REGS
/*
 * Call the interrupt handler.
 */
	jal	interrupt
	sw	a3, STAND_RA_OFFSET(sp)		# for debugging
/*
 * Restore registers and return from the interrupt.
 */
	mtc0	zero, COP_0_STATUS_REG
	nop
	nop
	nop
	li	v0, SR_EXL
	mtc0	v0, COP_0_STATUS_REG	# set exeption level bit.

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	v0, astpending			# any pending interrupts?
	mtc0	a0, COP_0_STATUS_REG	# Restore the SR, disable intrs
	bne	v0, zero, 1f			# dont restore, call softintr
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, COP_0_EXC_PC		# set return address
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
	li	t0, HARD_INT_MASK | SR_INT_ENAB
/*
 * Call the software interrupt handler.
 */
	jal	softintr
	mtc0	t0, COP_0_STATUS_REG	# enable interrupts (spl0)
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	mtc0	zero, COP_0_STATUS_REG
	nop
	nop
	nop
	li	v0, SR_EXL
	mtc0	v0, COP_0_STATUS_REG	# set exeption level bit.

	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, COP_0_STATUS_REG	# this should disable interrupts
	mtlo	t0
	mthi	t1
	lw	a0, UADDR+U_PCB_REGS+(PC * 4)
	lw	AT, UADDR+U_PCB_REGS+(AST * 4)
	lw	v0, UADDR+U_PCB_REGS+(V0 * 4)
	dmtc0	a0, COP_0_EXC_PC		# set return address
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
	eret
	.set	at
END(MipsUserIntr)

/*----------------------------------------------------------------------------
 *
 * MipsTLBInvalidException --
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
NLEAF(MipsTLBInvalidException)
	.set	noat
	dmfc0	k0, COP_0_BAD_VADDR	# get the fault address
	li	k1, (VM_MIN_KERNEL_ADDRESS)	# compute index
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

	mfc0	k0, COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0		# load PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, MipsKernGenException	# PTE invalid
	lw	k0, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO1		# load PTE entry
	nop
	tlbwi					# write TLB
	nop
	nop
	nop
	nop
	nop
	eret

KernTLBIOdd:
	mfc0	k0, COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of wired bit
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO1		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, MipsKernGenException	# PTE invalid
	lw	k0, -4(k1)			# get even PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0		# save PTE entry
	nop
	tlbwi					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
END(MipsTLBInvalidException)

/*----------------------------------------------------------------------------
 *
 * MipsTLBMissException --
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
NLEAF(MipsTLBMissException)
	.set	noat
	dmfc0	k0, COP_0_BAD_VADDR	# get the fault address
	li	k1, (VM_MIN_KERNEL_ADDRESS)	# compute index
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
	dmtc0	k0, COP_0_TLB_LO0		# load PTE entry
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, COP_0_TLB_LO1		# load PTE entry
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
	bne	k0, zero, MipsKernGenException	# Go panic
	nop

	la	a0, start - START_FRAME - 8	# set sp to a valid place
	sw	sp, 24(a0)
	move	sp, a0
	la	a0, 1f
	mfc0	a2, COP_0_STATUS_REG
	mfc0	a3, COP_0_CAUSE_REG
	dmfc0	a1, COP_0_EXC_PC
	sw	a2, 16(sp)
	sw	a3, 20(sp)
	move	a2, ra
	jal	printf
	dmfc0	a3, COP_0_BAD_VADDR
	.data
1:
	.asciiz	"ktlbmiss: PC %x RA %x ADR %x\nSR %x CR %x SP %x\n"
	.text

	la	sp, start - START_FRAME		# set sp to a valid place
	PANIC("kernel stack overflow")
	.set	at
END(MipsTLBMissException)

/*
 * Set/clear software interrupt routines.
 */

LEAF(setsoftclock)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_0	# set soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftclock)

LEAF(clearsoftclock)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_0	# clear soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftclock)

LEAF(setsoftnet)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_1	# set soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftnet)

LEAF(clearsoftnet)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_1	# clear soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftnet)

/*
 * Set/change interrupt priority routines.
 */

LEAF(MipsEnableIntr)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	or	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG	# enable all interrupts
	j	ra
	nop
END(MipsEnableIntr)

LEAF(spl0)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	or	t0, v0, (INT_MASK | SR_INT_ENAB)
	mtc0	t0, COP_0_STATUS_REG	# enable all interrupts
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(spl0)

LEAF(splsoftclock)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~SOFT_INT_MASK_0	# disable soft clock
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(splsoftclock)

LEAF(splsoftnet)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(splsoftnet)

LEAF(Mips_spl0)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_0|SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl0)

LEAF(Mips_spl1)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_1|SOFT_INT_MASK_0|SOFT_INT_MASK_1)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl1)

LEAF(Mips_spl2)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_2|SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl2)

LEAF(Mips_spl3)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_3|SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl3)

LEAF(Mips_spl4)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_4|SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl4)

LEAF(Mips_spl5)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~(INT_MASK_5|SOFT_INT_MASK_1|SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(Mips_spl5)

/*
 * We define an alternate entry point after mcount is called so it
 * can be used in mcount without causeing a recursive loop.
 */
LEAF(splhigh)
ALEAF(_splhigh)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	li	t0, ~SR_INT_ENAB	# disable all interrupts
	and	t0, t0, v0
	mtc0	t0, COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (INT_MASK | SR_INT_ENAB)
END(splhigh)

/*
 * Restore saved interrupt mask.
 */
LEAF(splx)
ALEAF(_splx)
	mfc0	v0, COP_0_STATUS_REG
	li	t0, ~(INT_MASK | SR_INT_ENAB)
	and	t0, t0, v0
	or	t0, t0, a0
	mtc0	t0, COP_0_STATUS_REG
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
	nop
	sync
	j	ra
	nop
END(wbflush)

/*--------------------------------------------------------------------------
 *
 * R4K_TLBWriteIndexed --
 *
 *	Write the given entry into the TLB at the given index.
 *
 *	R4K_TLBWriteIndexed(index, tlb)
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
LEAF(R4K_TLBWriteIndexed)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	nop
	lw	a2, 8(a1)
	lw	a3, 12(a1)
	dmfc0	t0, COP_0_TLB_HI		# Save the current PID.

	dmtc0	a2, COP_0_TLB_LO0		# Set up entry low0.
	dmtc0	a3, COP_0_TLB_LO1		# Set up entry low1.
	lw	a2, 0(a1)
	lw	a3, 4(a1)
	mtc0	a0, COP_0_TLB_INDEX	# Set the index.
	dmtc0	a2, COP_0_TLB_PG_MASK	# Set up entry mask.
	dmtc0	a3, COP_0_TLB_HI		# Set up entry high.
	nop
	tlbwi					# Write the TLB
	nop
	nop
	nop					# Delay for effect
	nop

	dmtc0	t0, COP_0_TLB_HI		# Restore the PID.
	nop
	dmtc0	zero, COP_0_TLB_PG_MASK	# Default mask value.
	j	ra
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
END(R4K_TLBWriteIndexed)

/*--------------------------------------------------------------------------
 *
 * R4K_SetPID --
 *
 *	Write the given pid into the TLB pid reg.
 *
 *	R4K_SetPID(pid)
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
LEAF(R4K_SetPID)
	dmtc0	a0, COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(R4K_SetPID)

/*--------------------------------------------------------------------------
 *
 * R4K_SetWIRED --
 *
 *	Write the given value into the TLB wired reg.
 *
 *	R4K_SetPID(wired)
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
LEAF(R4K_SetWIRED)
	mtc0	a0, COP_0_TLB_WIRED
	j	ra
	nop
END(R4K_SetWIRED)

/*--------------------------------------------------------------------------
 *
 * R4K_GetWIRED --
 *
 *	Get the value from the TLB wired reg.
 *
 *	R4K_GetWIRED(void)
 *
 * Results:
 *	Value of wired reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(R4K_GetWIRED)
	mfc0	v0, COP_0_TLB_WIRED
	j	ra
	nop
END(R4K_GetWIRED)

/*--------------------------------------------------------------------------
 *
 * R4K_TLBFlush --
 *
 *	Flush the "random" entries from the TLB.
 *	Uses "wired" register to determine what register to start with.
 *	Arg "tlbsize" is the number of entries to flush.
 *
 *	R4K_TLBFlush(tlbsize)
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(R4K_TLBFlush)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	mfc0	t1, COP_0_TLB_WIRED
	li	v0, CACHED_MEMORY_ADDR	# invalid address
	dmfc0	t0, COP_0_TLB_HI	# Save the PID

	dmtc0	v0, COP_0_TLB_HI	# Mark entry high as invalid
	dmtc0	zero, COP_0_TLB_LO0	# Zero out low entry0.
	dmtc0	zero, COP_0_TLB_LO1	# Zero out low entry1.
	mtc0	zero, COP_0_TLB_PG_MASK	# Zero out mask entry.
/*
 * Align the starting value (t1) and the upper bound (a0).
 */
1:
	mtc0	t1, COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, 1		# Increment index.
	tlbwi				# Write the TLB entry.
	nop
	nop
	bne	t1, a0, 1b
	nop

	dmtc0	t0, COP_0_TLB_HI	# Restore the PID
	j	ra
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
END(R4K_TLBFlush)


/*--------------------------------------------------------------------------
 *
 * R4K_TLBFlushAddr --
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 *	R4K_TLBFlushAddr(TLBhi)
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
LEAF(R4K_TLBFlushAddr)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	nop
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0		# Make shure valid hi value.
	dmfc0	t0, COP_0_TLB_HI	# Get current PID
	dmtc0	a0, COP_0_TLB_HI	# look for addr & PID
	nop
	nop
	nop
	tlbp				# Probe for the entry.
	nop
	nop				# Delay for effect
	nop
	mfc0	v0, COP_0_TLB_INDEX	# See what we got
	li	t1, CACHED_MEMORY_ADDR	# Load invalid entry.
	bltz	v0, 1f			# index < 0 => !found
	nop
	dmtc0	t1, COP_0_TLB_HI	# Mark entry high as invalid

	dmtc0	zero, COP_0_TLB_LO0	# Zero out low entry.
	dmtc0	zero, COP_0_TLB_LO1	# Zero out low entry.
	nop
	tlbwi
	nop
	nop
	nop
	nop
1:
	dmtc0	t0, COP_0_TLB_HI	# restore PID
	j	ra
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
END(R4K_TLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * R4K_TLBUpdate --
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 *	R4K_TLBUpdate(virpageadr, lowregx)
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
LEAF(R4K_TLBUpdate)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	and	t1, a0, 0x1000		# t1 = Even/Odd flag
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0
	dmfc0	t0, COP_0_TLB_HI	# Save current PID
	dmtc0	a0, COP_0_TLB_HI	# Init high reg
	and	a2, a1, PG_G		# Copy global bit
	nop
	nop
	tlbp				# Probe for the entry.
	dsll	a1, a1, 34
	dsrl	a1, a1, 34
	bne	t1, zero, 2f		# Decide even odd
	mfc0	v0, COP_0_TLB_INDEX	# See what we got
# EVEN
	nop
	bltz	v0, 1f			# index < 0 => !found
	nop

	tlbr				# update, read entry first
	nop
	nop
	nop
	dmtc0	a1, COP_0_TLB_LO0	# init low reg0.
	nop
	tlbwi				# update slot found
	b	4f
	nop
1:
	mtc0	zero, COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, COP_0_TLB_HI	# init high reg.
	dmtc0	a1, COP_0_TLB_LO0	# init low reg0.
	dmtc0	a2, COP_0_TLB_LO1	# init low reg1.
	nop
	tlbwr				# enter into a random slot
	b	4f
	nop
# ODD
2:
	nop
	bltz	v0, 3f			# index < 0 => !found
	nop

	tlbr				# read the entry first
	nop
	nop
	nop
	dmtc0	a1, COP_0_TLB_LO1	# init low reg1.
	nop
	tlbwi				# update slot found
	b	4f
	nop
3:
	mtc0	zero, COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, COP_0_TLB_HI	# init high reg.
	dmtc0	a2, COP_0_TLB_LO0	# init low reg0.
	dmtc0	a1, COP_0_TLB_LO1	# init low reg1.
	nop
	tlbwr				# enter into a random slot

4:					# Make shure pipeline
	nop				# advances before we
	nop				# uses the tlb.
	nop
	nop
	dmtc0	t0, COP_0_TLB_HI	# restore PID
	j	ra
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
END(R4K_TLBUpdate)

/*--------------------------------------------------------------------------
 *
 * R4K_TLBRead --
 *
 *	Read the TLB entry.
 *
 *	R4K_TLBRead(entry, tlb)
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
LEAF(R4K_TLBRead)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
	nop
	nop
	nop
	dmfc0	t0, COP_0_TLB_HI	# Get current PID

	mtc0	a0, COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr				# Read from the TLB
	nop
	nop
	nop
	mfc0	t2, COP_0_TLB_PG_MASK	# fetch the hi entry
	dmfc0	t3, COP_0_TLB_HI	# fetch the hi entry
	dmfc0	t4, COP_0_TLB_LO0	# See what we got
	dmfc0	t5, COP_0_TLB_LO1	# See what we got
	dmtc0	t0, COP_0_TLB_HI	# restore PID
	nop
	nop
	nop				# wait for PID active
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
	sw	t2, 0(a1)
	sw	t3, 4(a1)
	sw	t4, 8(a1)
	j	ra
	sw	t5, 12(a1)
END(R4K_TLBRead)

/*--------------------------------------------------------------------------
 *
 * R4K_TLBGetPID --
 *
 *	R4K_TLBGetPID()
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(R4K_TLBGetPID)
	dmfc0	v0, COP_0_TLB_HI	# get PID
	j	ra
	and	v0, v0, VMTLB_PID	# mask off PID
END(R4K_TLBGetPID)

/*--------------------------------------------------------------------------
 *
 * R4K_GetCOUNT --
 *
 *	R4K_GetCOUNT()
 *
 * Results:
 *	Returns the current COUNT reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(R4K_GetCOUNT)
	mfc0	v0, COP_0_COUNT
	nop	#???
	nop	#???
	j	ra
	nop
END(R4K_GetCOUNT)

/*--------------------------------------------------------------------------
 *
 * R4K_SetCOMPARE --
 *
 *	R4K_SetCOMPARE()
 *
 * Results:
 *	Sets a new value to the COMPARE register.
 *
 * Side effects:
 *	The COMPARE equal interrupt is acknowledged.
 *
 *--------------------------------------------------------------------------
 */
LEAF(R4K_SetCOMPARE)
	j	ra
	mtc0	a0, COP_0_COMPARE
END(R4K_SetCOMPARE)

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
LEAF(MipsSwitchFPState)
	mfc0	t1, COP_0_STATUS_REG	# Save old SR
	li	t0, SR_COP_1_BIT	# enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG

	beq	a0, zero, 1f		# skip save if NULL pointer
	nop
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	a0, P_ADDR(a0)		# get pointer to pcb for proc
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
	li	t3, ~SR_COP_1_BIT
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

	and	t0, t0, ~FPC_EXCEPTION_BITS
	ctc1	t0, FPC_CSR
	nop

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	j	ra
	nop
END(MipsSwitchFPState)

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
LEAF(MipsSaveCurFPState)
	lw	a0, P_ADDR(a0)			# get pointer to pcb for proc
	mfc0	t1, COP_0_STATUS_REG	# Disable interrupts and
	li	t0, SR_COP_1_BIT		#  enable the coprocessor
	mtc0	t0, COP_0_STATUS_REG
	sw	zero, machFPCurProcPtr		# indicate state has been saved
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	t2, U_PCB_REGS+(PS * 4)(a0)	# get CPU status register
	li	t3, ~SR_COP_1_BIT
	and	t2, t2, t3			# clear COP_1 enable bit
	cfc1	t0, FPC_CSR		# stall til FP done
	cfc1	t0, FPC_CSR		# now get status
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

	mtc0	t1, COP_0_STATUS_REG	# Restore the status register.
	j	ra
	nop
END(MipsSaveCurFPState)

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
NON_LEAF(MipsFPTrap, STAND_FRAME_SIZE, ra)
	subu	sp, sp, STAND_FRAME_SIZE
	mfc0	t0, COP_0_STATUS_REG
	sw	ra, STAND_RA_OFFSET(sp)
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)

	or	t1, t0, SR_COP_1_BIT
	mtc0	t1, COP_0_STATUS_REG
	nop
	nop
	nop
	nop
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
	sw	a2, STAND_FRAME_SIZE + 8(sp)
	li	a0, UADDR+U_PCB_REGS		# first arg is ptr to CPU registers
	move	a1, a2				# second arg is instruction PC
	move	a2, t1				# third arg is floating point CSR
	jal	MipsEmulateBranch		# compute PC after branch
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
 * PC (epc + 4) into v0 and continue to MipsEmulateFP().
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
	srl	a3, a0, OPCODE_SHIFT
	beq	a3, OPCODE_C1, 4f		# this should never fail
	nop
/*
 * Send a floating point exception signal to the current process.
 */
3:
	lw	a0, curproc			# get current process
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

/*
 * Turn off the floating point coprocessor and return.
 */
FPReturn:
	mfc0	t0, COP_0_STATUS_REG
	lw	ra, STAND_RA_OFFSET(sp)
	and	t0, t0, ~SR_COP_1_BIT
	mtc0	t0, COP_0_STATUS_REG
	j	ra
	addu	sp, sp, STAND_FRAME_SIZE
END(MipsFPTrap)

/*----------------------------------------------------------------------------
 *
 * R4K_ConfigCache --
 *
 *	Size the caches.
 *	NOTE: should only be called from mips_init().
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The size of the data cache is stored into CpuPrimaryDataCacheSize.
 *	The size of instruction cache is stored into CpuPrimaryInstCacheSize.
 *	Alignment mask for cache aliasing test is stored in CpuCacheAliasMask.
 *	cpu_id is set for later decision testing.

XXX Needs support for Cpu controlled L2 caches (SC cpus). XXX

 *
 *----------------------------------------------------------------------------
 */
LEAF(R4K_ConfigCache)
	.set	noreorder
	mfc0	t0, COP_0_PRID			# read processor ID register
	nop
	sw	t0, cpu_id			# save PRID register
	mfc0	v0, COP_0_CONFIG		# Get configuration register
	mfc0	v1, COP_0_PRID
	srl	t1, v0, 9			# Get I cache size.
	and	t1, 3
	li	t2, 4096
	sllv	t2, t2, t1
	sw	t2, CpuPrimaryInstCacheSize
	addiu	t2, -1
	and	t2, ~(NBPG - 1)
	sw	t2, CpuCacheAliasMask

	and	t2, v0, 0x20
	srl	t2, t2, 1
	addu	t2, t2, 16
	sw	t2, CpuPrimaryInstCacheLSize

	srl	t1, v0, 6			# Get D cache size.
	and	t1, 3
	li	t2, 4096
	sllv	t2, t2, t1
	sw	t2, CpuPrimaryDataCacheSize

	and	t2, v0, 0x10
	addu	t2, t2, 16
	sw	t2, CpuPrimaryDataCacheLSize
	and	v1, 0xff00
	li	t2, 1				# Recognize CPU's with
	li	t1, (MIPS_R4600 << 8)		# two way associative caches.
	beq	v1, t1, 1f
	li	t1, (MIPS_R4700 << 8)
	beq	v1, t1, 1f
	li	t1, (MIPS_R5000 << 8)
	beq	v1, t1, 1f
	li	t1, (MIPS_RM52X0 << 8)
	beq	v1, t1, 1f
	li	t2, 1

	li	t2, 0
1:
	sw	t2, CpuTwoWayCache
	j	ra
	nop
END(R4K_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * R4K_FlushCache --
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
LEAF(R4K_FlushCache)
	.set	noreorder
/*XXX 4600 Bug */
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
/*XXX*/
	lw	v0, CpuTwoWayCache
	lw	t1, CpuPrimaryInstCacheSize
	lw	t2, CpuPrimaryDataCacheSize
 #	lw	t3, CpuPrimaryInstCacheLSize
 #	lw	t4, CpuPrimaryDataCacheLSize
/*
 * Flush the instruction cache.
 */
	li	t0, CACHED_MEMORY_ADDR
	addu	t1, t0, t1		# End address
	subu	t1, t1, 128
1:
	bne	v0, zero, 2f
	cache	0, 0(t0)

	cache	0, 16(t0)
	cache	0, 48(t0)
	cache	0, 80(t0)
	b	3f
	cache	0, 112(t0)

2:
	cache	0, 8192+0(t0)
	cache	0, 8192+32(t0)
	cache	0, 8192+64(t0)
	cache	0, 8192+96(t0)

3:
	cache	0, 32(t0)
	cache	0, 64(t0)
	cache	0, 96(t0)
	bne	t0, t1, 1b
	addu	t0, t0, 128

/*
 * Flush the data cache.
 */
	li	t0, CACHED_MEMORY_ADDR
	addu	t1, t0, t2		# End address
	subu	t1, t1, 128
1:
	bne	v0, zero, 2f
	cache	1, 0(t0)

	cache	1, 16(t0)
	cache	1, 48(t0)
	cache	1, 80(t0)
	b	3f
	cache	1, 112(t0)

2:
	cache	1, 8192+0(t0)
	cache	1, 8192+32(t0)
	cache	1, 8192+64(t0)
	cache	1, 8192+96(t0)

3:
	cache	1, 32(t0)
	cache	1, 64(t0)
	cache	1, 96(t0)
	bne	t0, t1, 1b
	addu	t0, t0, 128

/*XXX 4600 Bug */
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register.
/*XXX*/
	j	ra
	nop
END(R4K_FlushCache)

/*----------------------------------------------------------------------------
 *
 * R4K_FlushICache --
 *
 *	void R4K_FlushICache(addr, len)
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
LEAF(R4K_FlushICache)
/*XXX 4600 Bug */
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
/*XXX*/
	lw	v0, CpuTwoWayCache
	addu	a1, 127		# Align
	srl	a1, a1, 7	# Number of unrolled loops
1:
	bne	v0, zero, 2f
	addu	a1, -1

	cache	0, 16(a0)
	cache	0, 48(a0)
	cache	0, 80(a0)
	b	3f
	cache	0, 112(a0)

2:
	cache	0, 8192+0(a0)
	cache	0, 8192+32(a0)
	cache	0, 8192+64(a0)
	cache	0, 8192+96(a0)

3:
	cache	0, 0(a0)
	cache	0, 32(a0)
	cache	0, 64(a0)
	cache	0, 96(a0)
	bne	a1, zero, 1b
	addu	a0, 128

/*XXX 4600 Bug */
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register.
/*XXX*/
	j	ra
	move	v0, zero		# suiword depends on this!!
END(R4K_FlushICache)

/*----------------------------------------------------------------------------
 *
 * R4K_FlushDCache --
 *
 *	void R4K_FlushDCache(addr, len)
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
LEAF(R4K_FlushDCache)
/*XXX 4600 Bug */
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, COP_0_STATUS_REG	# Disable interrupts
/*XXX*/
	lw	v0, CpuTwoWayCache
	lw	a2, CpuPrimaryDataCacheSize
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
	bne	v0, zero, 2f
	addu	a1, -1

	cache	1, 16(a0)
	cache	1, 48(a0)
	cache	1, 80(a0)
	b	3f
	cache	1, 112(a0)

2:
	cache	1, 8192+0(a0)
	cache	1, 8192+32(a0)
	cache	1, 8192+64(a0)
	cache	1, 8192+96(a0)

3:
	cache	1, 0(a0)
	cache	1, 32(a0)
	cache	1, 64(a0)
	cache	1, 96(a0)
	bne	a1, zero, 1b
	addu	a0, 128

/*XXX 4600 Bug */
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register.
/*XXX*/
	j	ra
	nop
END(R4K_FlushDCache)

/*----------------------------------------------------------------------------
 *
 * R4K_HitFlushDCache --
 *
 *	void R4K_HitFlushDCache(addr, len)
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
LEAF(R4K_HitFlushDCache)
	lw	v0, CpuTwoWayCache
	beq	a1, zero, 3f
	addu	a1, 127		# Align
	addu	a1, a1, a0
	and	a0, a0, -128
	subu	a1, a1, a0
	srl	a1, a1, 7	# Compute number of cache lines
1:
	bne	v0, zero, 2f
	addu	a1, -1

	cache	0x15, 16(a0)
	cache	0x15, 48(a0)
	cache	0x15, 80(a0)
	cache	0x15, 112(a0)

2:
	cache	0x15, 0(a0)
	cache	0x15, 32(a0)
	cache	0x15, 64(a0)
	cache	0x15, 96(a0)
	bne	a1, zero, 1b
	addu	a0, 128

3:
	j	ra
	nop
END(R4K_HitFlushDCache)
/*----------------------------------------------------------------------------
 *
 * R4K_InvalidateDCache --
 *
 *	void R4K_InvalidateDCache(addr, len)
 *		vm_offset_t addr, len;
 *
 *	Invalidate data cache for range of addr to addr + len - 1.
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
LEAF(R4K_InvalidateDCache)
	addu	a1, a1, a0			# compute ending address
1:
	cache	0x11, 0(a0)
	cache	0x11, 32(a0)
	cache	0x11, 64(a0)
	cache	0x11, 96(a0)
	bne	a0, a1, 1b
	addu	a0, a0, 128

	j	ra
	nop
END(R4K_InvalidateDCache)



#if defined(DDB) || defined(DEBUG)
LEAF(kdbpeek)
	li	v0, DDBERR
	and	v0, a0, 3			# unaligned ?
	bne	v0, zero, 1f
	sw	v0, UADDR+U_PCB_ONFAULT

	lw	v0, (a0)
	jr	ra
	sw	zero, UADDR+U_PCB_ONFAULT

1:
	LWHI	v0, 0(a0)
	LWLO	v0, 3(a0)
	jr	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(kdbpeek)

ddberr:
	jr	ra
	nop

#if defined(DDB)
LEAF(kdbpoke)
	li	v0, DDBERR
	and	v0, a0, 3			# unaligned ?
	bne	v0, zero, 1f
	sw	v0, UADDR+U_PCB_ONFAULT

	sw	a1, (a0)
	jr	ra
	sw	zero, UADDR+U_PCB_ONFAULT

1:
	SWHI	a1, 0(a0)
	SWLO	a1, 3(a0)
	jr	ra
	sw	zero, UADDR+U_PCB_ONFAULT
END(kdbpoke)

LEAF(Debugger)
	break   BREAK_SOVER_VAL
	jr	ra
	nop
END(Debugger)

LEAF(setjmp)
	mfc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	STORE	s0, RSIZE * 0(a0)
	STORE	s1, RSIZE * 1(a0)
	STORE	s2, RSIZE * 2(a0)
	STORE	s3, RSIZE * 3(a0)
	STORE	s4, RSIZE * 4(a0)
	STORE	s5, RSIZE * 5(a0)
	STORE	s6, RSIZE * 6(a0)
	STORE	s7, RSIZE * 7(a0)
	STORE	s8, RSIZE * 8(a0)
	STORE	sp, RSIZE * 9(a0)
	STORE	ra, RSIZE * 10(a0)
	STORE	v0, RSIZE * 11(a0)
	jr	ra
	li	v0, 0			# setjmp return
END(setjmp)

LEAF(longjmp)
	LOAD	v0, RSIZE * 11(a0)
	LOAD	ra, RSIZE * 10(a0)
	LOAD	s0, RSIZE * 0(a0)
	LOAD	s1, RSIZE * 1(a0)
	LOAD	s2, RSIZE * 2(a0)
	LOAD	s3, RSIZE * 3(a0)
	LOAD	s4, RSIZE * 4(a0)
	LOAD	s5, RSIZE * 5(a0)
	LOAD	s6, RSIZE * 6(a0)
	LOAD	s7, RSIZE * 7(a0)
	LOAD	s8, RSIZE * 8(a0)
	LOAD	sp, RSIZE * 9(a0)
	mtc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	jr	ra
	li	v0, 1			# longjmp return
END(longjmp)

	.data
	.globl	esym
esym:	.word	0

#endif /* DDB */
#endif /* DDB || DEBUGGER */

/*
 * Interrupt counters for vmstat.
 */
	.data
	.globl intrcnt
	.globl eintrcnt
	.globl intrnames
	.globl eintrnames
intrnames:
	.asciiz	"softclock"
	.asciiz	"softnet"
	.asciiz	"local_dma"
	.asciiz	"local_dev"
	.asciiz	"isa_dev"
	.asciiz	"isa_nmi"
	.asciiz	"clock"
	.asciiz	"statclock"
eintrnames:
	.align	3
intrcnt:
	.word	0,0,0,0,0,0,0,0
eintrcnt:
