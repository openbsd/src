/*	$NetBSD: locore.S,v 1.21 1996/05/19 00:25:14 jonathan Exp $	*/

/*
 * locore file for either "new" merged MIPS-I/MIPS-II kernels or
 * old-style pmax kernels
 */

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

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/regnum.h>
#include <machine/machAsmDefs.h>
#include <machine/pte.h>

#include "assym.h"

	.set	noreorder

/*
 * Amount to take off of the stack for the benefit of the debugger.
 */
#define START_FRAME	((4 * 4) + 4 + 4)

	.globl	start
start:
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	li	t1, MACH_CACHED_MEMORY_ADDR	# invalid address
	mtc0	t1, MACH_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MACH_COP_0_TLB_LOW	# Zero out low entry.
/*
 * Clear the TLB (just to be safe).
 * Align the starting value (t1), the increment (t2) and the upper bound (t3).
 */
	move	t1, zero
	li	t2, 1 << VMMACH_TLB_INDEX_SHIFT
	li	t3, VMMACH_NUM_TLB_ENTRIES << VMMACH_TLB_INDEX_SHIFT
1:
	mtc0	t1, MACH_COP_0_TLB_INDEX	# Set the index register.
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

	li	t0, MACH_SR_COP_1_BIT		# Disable interrupts and
	mtc0	t0, MACH_COP_0_STATUS_REG	#   enable the fp coprocessor
	li	sp, KERNELSTACK - START_FRAME	# switch to standard stack
	mfc0	t0, MACH_COP_0_PRID		# read processor ID register

	nop					# XXX r4000 pipeline:
	nop					# wait for new status to
	nop					# to be effective
	nop
	cfc1	t1, MACH_FPC_ID			# read FPU ID register
	sw	t0, _C_LABEL(cpu_id)		# save PRID register
	sw	t1, _C_LABEL(fpu_id)		# save FPU ID register
	jal	_C_LABEL(main)			# main(regs)
	move	a0, zero
/*
 * proc[1] == /etc/init now running here.
 * Restore user registers and return.
 */
	.set	noat
	li	v0, PSL_USERSET
	mtc0	v0, MACH_COP_0_STATUS_REG	# switch to user mode
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
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

/*
 * This code is copied the user's stack for returning from signal handlers
 * (see sendsig() and sigreturn()). We have to compute the address
 * of the sigcontext struct for the sigreturn call.
 */
	.globl	_C_LABEL(sigcode)
_C_LABEL(sigcode):
	addu	a0, sp, 16		# address of sigcontext
	li	v0, SYS_sigreturn	# sigreturn(scp)
	syscall
	break	0			# just in case sigreturn fails
	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):

/*
 * Primitives
 */

/*
 * This table is indexed by u.u_pcb.pcb_onfault in trap().
 * The reason for using this table rather than storing an address in
 * u.u_pcb.pcb_onfault is simply to make the code faster.
 */
	.globl	_C_LABEL(onfault_table)
	.data
	.align	2
_C_LABEL(onfault_table):
	.word	0		# invalid index number
#define BADERR		1
	.word	baderr
#define COPYERR		2
	.word	_C_LABEL(copyerr)
#define FSWBERR		3
	.word	_C_LABEL(fswberr)
#define FSWINTRBERR	4
	.word	_C_LABEL(fswintrberr)
#ifdef KADB
#define KADBERR		5
	.word	kadberr
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
 * Copy data to the DMA buffer.
 * The DMA bufffer can only be written one short at a time
 * (and takes ~14 cycles).
 *
 *	CopyToBuffer(src, dst, length)
 *		u_short *src;	NOTE: must be short aligned
 *		u_short *dst;
 *		int length;
 */
LEAF(CopyToBuffer)
	blez	a2, 2f
	nop
1:
	lhu	t0, 0(a0)		# read 2 bytes of data
	subu	a2, a2, 2
	addu	a0, a0, 2
	addu	a1, a1, 4
	bgtz	a2, 1b
	sh	t0, -4(a1)		# write 2 bytes of data to buffer
2:
	j	ra
	nop
END(CopyToBuffer)

/*
 * Copy data from the DMA buffer.
 * The DMA bufffer can only be read one short at a time
 * (and takes ~12 cycles).
 *
 *	CopyFromBuffer(src, dst, length)
 *		u_short *src;
 *		char *dst;
 *		int length;
 */
LEAF(CopyFromBuffer)
	and	t0, a1, 1		# test for aligned dst
	beq	t0, zero, 3f
	nop
	blt	a2, 2, 7f		# at least 2 bytes to copy?
	nop
1:
	lhu	t0, 0(a0)		# read 2 bytes of data from buffer
	addu	a0, a0, 4		# keep buffer pointer word aligned
	addu	a1, a1, 2
	subu	a2, a2, 2
	sb	t0, -2(a1)
	srl	t0, t0, 8
	bge	a2, 2, 1b
	sb	t0, -1(a1)
3:
	blt	a2, 2, 7f		# at least 2 bytes to copy?
	nop
6:
	lhu	t0, 0(a0)		# read 2 bytes of data from buffer
	addu	a0, a0, 4		# keep buffer pointer word aligned
	addu	a1, a1, 2
	subu	a2, a2, 2
	bge	a2, 2, 6b
	sh	t0, -2(a1)
7:
	ble	a2, zero, 9f		# done?
	nop
	lhu	t0, 0(a0)		# copy one more byte
	nop
	sb	t0, 0(a1)
9:
	j	ra
	nop
END(CopyFromBuffer)

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
	mfc0	v0, MACH_COP_0_STATUS_REG
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
	lw	t2, _C_LABEL(whichqs)	# check corresponding bit
	sll	t1, t1, t0
	and	v0, t2, t1
	sw	ra, STAND_RA_OFFSET(sp)	##
	bne	v0, zero, 1f		##
	lw	v0, P_BACK(a0)		# v0 = p->p_back
	PANIC("remrunqueue")			## it wasnt recorded to be on its q
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
	mtc0	zero, MACH_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MACH_COP_0_TLB_HI		# init high entry
	mtc0	t0, MACH_COP_0_TLB_LOW		# init low entry
	li	t0, 1 << VMMACH_TLB_INDEX_SHIFT
	tlbwi					# Write the TLB entry.
	addu	v0, v0, NBPG			# 2nd HI entry
	mtc0	t0, MACH_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MACH_COP_0_TLB_HI		# init high entry
	mtc0	t1, MACH_COP_0_TLB_LOW		# init low entry
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
	li	t0, (MACH_INT_MASK | MIPS_SR_INT_IE)
	mtc0	t0, MACH_COP_0_STATUS_REG	# enable all interrupts
	sw	zero, _C_LABEL(curproc)		# set curproc NULL for stats
1:
	lw	t0, _C_LABEL(whichqs)		# look for non-empty queue
	nop
	beq	t0, zero, 1b
	nop
	b	sw1
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable all interrupts
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
	mfc0	t0, MACH_COP_0_STATUS_REG	# t0 = saved status register
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
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable all interrupts
sw1:
	nop					# wait for intrs disabled
	nop
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
	sll	v0, v0, VMMACH_TLB_PID_SHIFT	# v0 = aligned PID
	lw	t0, P_UPTE+0(s0)		# t0 = first u. pte
	lw	t1, P_UPTE+4(s0)		# t1 = 2nd u. pte
	or	v0, v0, UADDR			# v0 = first HI entry
/*
 * Resume process indicated by the pte's for its u struct
 * NOTE: This is hard coded to UPAGES == 2.
 * Also, there should be no TLB faults at this point.
 */
	mtc0	zero, MACH_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MACH_COP_0_TLB_HI		# init high entry
	mtc0	t0, MACH_COP_0_TLB_LOW		# init low entry
	li	t0, 1 << VMMACH_TLB_INDEX_SHIFT
	tlbwi					# Write the TLB entry.
	addu	v0, v0, NBPG			# 2nd HI entry
	mtc0	t0, MACH_COP_0_TLB_INDEX	# set the index register
	mtc0	v0, MACH_COP_0_TLB_HI		# init high entry
	mtc0	t1, MACH_COP_0_TLB_LOW		# init low entry
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
	mtc0	v0, MACH_COP_0_STATUS_REG
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
	b	_C_LABEL(MachFlushICache)	# NOTE: this should not clobber v0!
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

/*
 *----------------------------------------------------------------------------
 *
 * mips_r2000_UTLBmiss --
 * MachUTLBmiss --
 *
 *	Vector code for a MIPS-I user-space TLB miss from user-space.
 *
 *
 * This code is copied to the UTLB exception vector address to
 * handle user level TLB translation misses.
 * NOTE: This code must be relocatable!!!
 */
	.globl	_C_LABEL(mips_R2000_UTLBMiss)
_C_LABEL(mips_R2000_UTLBMiss):
	.globl	_C_LABEL(MachUTLBMiss)
_C_LABEL(MachUTLBMiss):
	.set	noat
	mfc0	k0, MACH_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, UADDR+U_PCB_SEGTAB		# get the current segment table
	bltz	k0, 1f				# R3000 chip bug
	srl	k0, k0, SEGSHIFT		# compute segment table index
	sll	k0, k0, 2
	addu	k1, k1, k0
	mfc0	k0, MACH_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, 0(k1)			# get pointer to segment map
	srl	k0, k0, PGSHIFT - 2		# compute segment map index
	andi	k0, k0, (NPTEPG - 1) << 2
	beq	k1, zero, 2f			# invalid segment map
	addu	k1, k1, k0			# index into segment map
	lw	k0, 0(k1)			# get page PTE
	nop
	beq	k0, zero, 2f			# dont load invalid entries
	mtc0	k0, MACH_COP_0_TLB_LOW
	mfc0	k1, MACH_COP_0_EXC_PC		# get return address
	tlbwr					# update TLB
	j	k1
	rfe
1:
	mfc0	k1, MACH_COP_0_EXC_PC		# get return address
	nop
	j	k1
	rfe
2:
	j	SlowFault			# handle the rest
	nop
	.set	at
	.globl	_C_LABEL(MachUTLBMissEnd)
_C_LABEL(MachUTLBMissEnd):

	.globl	_C_LABEL(mips_R2000_UTLBMissEnd)
_C_LABEL(mips_R2000_UTLBMissEnd):


/*
 *----------------------------------------------------------------------------
 *
 * mips_R2000_execption --
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
	.globl	_C_LABEL(mips_R2000_exception)
_C_LABEL(mips_R2000_exception):
/*
 * Find out what mode we came from and jump to the proper handler.
 */
	.set	noat
	mfc0	k0, MACH_COP_0_STATUS_REG	# Get the status register
	mfc0	k1, MACH_COP_0_CAUSE_REG	# Get the cause register value.
	and	k0, k0, MIPS_3K_SR_KU_PREV	# test for user mode
	sll	k0, k0, 4			# shift user bit for cause index
	and	k1, k1, MIPS_3K_CR_EXC_CODE	# Mask out the cause bits.
	or	k1, k1, k0			# change index to user table
1:
	la	k0, _C_LABEL(machExceptionTable) # get base of the jump table
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
	.globl	_C_LABEL(mips_R2000_exceptionEnd)
_C_LABEL(mips_R2000_exceptionEnd):



#ifdef R4000	/* XXX doesn't assemble in default pmax kernel */

/*
 *----------------------------------------------------------------------------
 *
 * mips_R4000_TLBMiss --
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
	.globl	_C_LABEL(mips_R4000_TLBMiss)
_C_LABEL(mips_R4000_TLBMiss):
	.globl	_C_LABEL(MachTLBMiss)
_C_LABEL(MachTLBMiss):
	.set	noat
	dmfc0	k0, MACH_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, UADDR+U_PCB_SEGTAB		# get the current segment table
	bltz	k0, 1f				# kernel address space ->
	srl	k0, k0, SEGSHIFT - 2		# compute segment table index
	andi	k0, k0, 0x7fc			# PMAP_SEGTABSIZ-1
	addu	k1, k1, k0
	dmfc0	k0, MACH_COP_0_BAD_VADDR	# get the virtual address
	lw	k1, 0(k1)			# get pointer to segment map
	srl	k0, k0, PGSHIFT - 2		# compute segment map index
	andi	k0, k0, ((NPTEPG/2) - 1) << 3
	beq	k1, zero, 2f			# invalid segment map
	addu	k1, k1, k0			# index into segment map
	lw	k0, 0(k1)			# get page PTE
	lw	k1, 4(k1)
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MACH_COP_0_TLB_LO0
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, MACH_COP_0_TLB_LO1
	nop
	tlbwr					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
1:
	j	MachTLBMissException
	nop
2:
	j	SlowFault
	nop

	.globl	_C_LABEL(MachTLBMissEnd)
C_LABEL(MachTLBMissEnd):
	.globl	_C_LABEL(mips_R4000_TLBMissEnd)
_C_LABEL(mips_R4000_TLBMissEnd):
	.set	at
#endif	/* XXX doesn't assemble in default pmax kernel *//*


 *----------------------------------------------------------------------------
 *
 * Mips_R4000_execption --
 *
 *	Vector code for the general exception vector 0x80000080
 *	on an r4000 or r4400.
 *
 * This code is copied to the general exception vector address to
 * handle all execptions except RESET and TLBMiss.
 * NOTE: This code must be relocatable!!!
 *----------------------------------------------------------------------------
 */
	.globl	mips_r4000_exception
_C_LABEL(mips_R4000_exception):
/*
 * Find out what mode we came from and jump to the proper handler.
 */
	.set	noat
	mfc0	k0, MACH_COP_0_STATUS_REG	# Get the status register
	mfc0	k1, MACH_COP_0_CAUSE_REG	# Get the cause register value.
	and	k0, k0, MIPS_4K_SR_KSU_USER	# test for user mode
						# sneaky but the bits are
						# with us........
	sll	k0, k0, 3			# shift user bit for cause index
	and	k1, k1, MIPS_4K_CR_EXC_CODE	# Mask out the cause bits.
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
	.globl	mips_R4000_exceptionEnd
_C_LABEL(mips_R4000_exceptionEnd):


/*
 * We couldn't find a TLB entry.
 * Find out what mode we came from and call the appropriate handler.
 */
SlowFault:
	.set	noat
	mfc0	k0, MACH_COP_0_STATUS_REG
	nop
	and	k0, k0, MACH_SR_KU_PREV
	bne	k0, zero, _C_LABEL(MachUserGenException)
	nop
	.set	at
/*
 * Fall though ...
 */

/*----------------------------------------------------------------------------
 *
 * MachKernGenException --
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

NNON_LEAF(MachKernGenException, KERN_EXC_FRAME_SIZE, ra)
ALEAF(mips_r2000_KernGenException)
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
	mfc0	a0, MACH_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, KERN_REG_OFFSET + 32(sp)
	sw	t2, KERN_REG_OFFSET + 36(sp)
	sw	t3, KERN_REG_OFFSET + 40(sp)
	sw	t4, KERN_REG_OFFSET + 44(sp)
	mfc0	a1, MACH_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, KERN_REG_OFFSET + 48(sp)
	sw	t6, KERN_REG_OFFSET + 52(sp)
	sw	t7, KERN_REG_OFFSET + 56(sp)
	sw	t8, KERN_REG_OFFSET + 60(sp)
	mfc0	a2, MACH_COP_0_BAD_VADDR	# Third arg is the fault addr.
	sw	t9, KERN_REG_OFFSET + 64(sp)
	sw	ra, KERN_REG_OFFSET + 68(sp)
	sw	v0, KERN_MULT_LO_OFFSET(sp)
	sw	v1, KERN_MULT_HI_OFFSET(sp)
	mfc0	a3, MACH_COP_0_EXC_PC		# Fourth arg is the pc.
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
	mtc0	a0, MACH_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	move	k0, v0
	lw	AT, KERN_REG_OFFSET + 0(sp)
	lw	v0, KERN_REG_OFFSET + 4(sp)
	lw	v1, KERN_REG_OFFSET + 8(sp)
	lw	a0, KERN_REG_OFFSET + 12(sp)
	lw	a1, KERN_REG_OFFSET + 16(sp)
	lw	a2, KERN_REG_OFFSET + 20(sp)
	lw	a3, KERN_REG_OFFSET + 24(sp)
	lw	t0, KERN_REG_OFFSET + 28(sp)
	lw	t1, KERN_REG_OFFSET + 32(sp)
	lw	t2, KERN_REG_OFFSET + 36(sp)
	lw	t3, KERN_REG_OFFSET + 40(sp)
	lw	t4, KERN_REG_OFFSET + 44(sp)
	lw	t5, KERN_REG_OFFSET + 48(sp)
	lw	t6, KERN_REG_OFFSET + 52(sp)
	lw	t7, KERN_REG_OFFSET + 56(sp)
	lw	t8, KERN_REG_OFFSET + 60(sp)
	lw	t9, KERN_REG_OFFSET + 64(sp)
	lw	ra, KERN_REG_OFFSET + 68(sp)
	addu	sp, sp, KERN_EXC_FRAME_SIZE
	j	k0				# Now return from the
	rfe					#  exception.
	.set	at
END(MachKernGenException)

/*----------------------------------------------------------------------------
 *
 * MachUserGenException --
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
NNON_LEAF(MachUserGenException, STAND_FRAME_SIZE, ra)
ALEAF(mips_r2000_UserGenException)
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
	mfc0	a0, MACH_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	s0, UADDR+U_PCB_REGS+(S0 * 4)
	mfc0	a1, MACH_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	s1, UADDR+U_PCB_REGS+(S1 * 4)
	sw	s2, UADDR+U_PCB_REGS+(S2 * 4)
	sw	s3, UADDR+U_PCB_REGS+(S3 * 4)
	sw	s4, UADDR+U_PCB_REGS+(S4 * 4)
	mfc0	a2, MACH_COP_0_BAD_VADDR	# Third arg is the fault addr
	sw	s5, UADDR+U_PCB_REGS+(S5 * 4)
	sw	s6, UADDR+U_PCB_REGS+(S6 * 4)
	sw	s7, UADDR+U_PCB_REGS+(S7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a3, MACH_COP_0_EXC_PC		# Fourth arg is the pc.
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
	and	t0, a0, ~MACH_SR_COP_1_BIT	# Turn off the FPU.
	.set	noat
/*
 * Call the exception handler.
 */
	jal	_C_LABEL(trap)
	mtc0	t0, MACH_COP_0_STATUS_REG
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MACH_COP_0_STATUS_REG	# this should disable interrupts
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
END(MachUserGenException)

/*----------------------------------------------------------------------------
 *
 * MachKernIntr --
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

NNON_LEAF(MachKernIntr, KINTR_FRAME_SIZE, ra)
ALEAF(mips_r2000_KernIntr)
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
	mfc0	a0, MACH_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, KINTR_REG_OFFSET + 32(sp)
	sw	t2, KINTR_REG_OFFSET + 36(sp)
	sw	t3, KINTR_REG_OFFSET + 40(sp)
	sw	t4, KINTR_REG_OFFSET + 44(sp)
	mfc0	a1, MACH_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, KINTR_REG_OFFSET + 48(sp)
	sw	t6, KINTR_REG_OFFSET + 52(sp)
	sw	t7, KINTR_REG_OFFSET + 56(sp)
	sw	t8, KINTR_REG_OFFSET + 60(sp)
	mfc0	a2, MACH_COP_0_EXC_PC		# Third arg is the pc.
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
	mtc0	a0, MACH_COP_0_STATUS_REG	# Restore the SR, disable intrs
	mtlo	t0
	mthi	t1
	lw	k0, STAND_RA_OFFSET(sp)
	lw	AT, KINTR_REG_OFFSET + 0(sp)
	lw	v0, KINTR_REG_OFFSET + 4(sp)
	lw	v1, KINTR_REG_OFFSET + 8(sp)
	lw	a0, KINTR_REG_OFFSET + 12(sp)
	lw	a1, KINTR_REG_OFFSET + 16(sp)
	lw	a2, KINTR_REG_OFFSET + 20(sp)
	lw	a3, KINTR_REG_OFFSET + 24(sp)
	lw	t0, KINTR_REG_OFFSET + 28(sp)
	lw	t1, KINTR_REG_OFFSET + 32(sp)
	lw	t2, KINTR_REG_OFFSET + 36(sp)
	lw	t3, KINTR_REG_OFFSET + 40(sp)
	lw	t4, KINTR_REG_OFFSET + 44(sp)
	lw	t5, KINTR_REG_OFFSET + 48(sp)
	lw	t6, KINTR_REG_OFFSET + 52(sp)
	lw	t7, KINTR_REG_OFFSET + 56(sp)
	lw	t8, KINTR_REG_OFFSET + 60(sp)
	lw	t9, KINTR_REG_OFFSET + 64(sp)
	lw	ra, KINTR_REG_OFFSET + 68(sp)
	addu	sp, sp, KINTR_FRAME_SIZE
	j	k0				# Now return from the
	rfe					#  interrupt.
	.set	at
END(MachKernIntr)

/*----------------------------------------------------------------------------
 *
 * MachUserIntr --
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
NNON_LEAF(MachUserIntr, STAND_FRAME_SIZE, ra)
ALEAF(mips_r2000_UserIntr)
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
	mfc0	a0, MACH_COP_0_STATUS_REG	# First arg is the status reg.
	sw	t1, UADDR+U_PCB_REGS+(T1 * 4)
	sw	t2, UADDR+U_PCB_REGS+(T2 * 4)
	sw	t3, UADDR+U_PCB_REGS+(T3 * 4)
	sw	t4, UADDR+U_PCB_REGS+(T4 * 4)
	mfc0	a1, MACH_COP_0_CAUSE_REG	# Second arg is the cause reg.
	sw	t5, UADDR+U_PCB_REGS+(T5 * 4)
	sw	t6, UADDR+U_PCB_REGS+(T6 * 4)
	sw	t7, UADDR+U_PCB_REGS+(T7 * 4)
	sw	t8, UADDR+U_PCB_REGS+(T8 * 4)
	mfc0	a2, MACH_COP_0_EXC_PC		# Third arg is the pc.
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
	and	t0, a0, ~MACH_SR_COP_1_BIT	# Turn off the FPU.
	.set	noat
	mtc0	t0, MACH_COP_0_STATUS_REG
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
	mtc0	a0, MACH_COP_0_STATUS_REG	# Restore the SR, disable intrs
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
	li	t0, MACH_HARD_INT_MASK | MIPS_SR_INT_IE
/*
 * Call the software interrupt handler.
 */
	jal	_C_LABEL(softintr)
	mtc0	t0, MACH_COP_0_STATUS_REG	# enable interrupts (spl0)
/*
 * Restore user registers and return. NOTE: interrupts are enabled.
 */
	lw	a0, UADDR+U_PCB_REGS+(SR * 4)
	lw	t0, UADDR+U_PCB_REGS+(MULLO * 4)
	lw	t1, UADDR+U_PCB_REGS+(MULHI * 4)
	mtc0	a0, MACH_COP_0_STATUS_REG	# this should disable interrupts
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
END(MachUserIntr)



/*
 * Set/clear software interrupt routines.
 */

LEAF(setsoftclock)
	mfc0	v1, MACH_COP_0_STATUS_REG	# save status register
	mtc0	zero, MACH_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	mfc0	v0, MACH_COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, MACH_SOFT_INT_MASK_0	# set soft clock interrupt
	mtc0	v0, MACH_COP_0_CAUSE_REG	# save it
	mtc0	v1, MACH_COP_0_STATUS_REG
	j	ra
	nop
END(setsoftclock)

LEAF(clearsoftclock)
	mfc0	v1, MACH_COP_0_STATUS_REG	# save status register
	mtc0	zero, MACH_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MACH_COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~MACH_SOFT_INT_MASK_0	# clear soft clock interrupt
	mtc0	v0, MACH_COP_0_CAUSE_REG	# save it
	mtc0	v1, MACH_COP_0_STATUS_REG
	j	ra
	nop
END(clearsoftclock)

LEAF(setsoftnet)
	mfc0	v1, MACH_COP_0_STATUS_REG	# save status register
	mtc0	zero, MACH_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MACH_COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, MACH_SOFT_INT_MASK_1	# set soft net interrupt
	mtc0	v0, MACH_COP_0_CAUSE_REG	# save it
	mtc0	v1, MACH_COP_0_STATUS_REG
	j	ra
	nop
END(setsoftnet)

LEAF(clearsoftnet)
	mfc0	v1, MACH_COP_0_STATUS_REG	# save status register
	mtc0	zero, MACH_COP_0_STATUS_REG	# disable interrupts (2 cycles)
	nop
	nop
	mfc0	v0, MACH_COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~MACH_SOFT_INT_MASK_1	# clear soft net interrupt
	mtc0	v0, MACH_COP_0_CAUSE_REG	# save it
	mtc0	v1, MACH_COP_0_STATUS_REG
	j	ra
	nop
END(clearsoftnet)

/*
 * Set/change interrupt priority routines.
 */

LEAF(MachEnableIntr)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	nop
	or	v0, v0, MIPS_SR_INT_IE
	mtc0	v0, MACH_COP_0_STATUS_REG	# enable all interrupts
	j	ra
	nop
END(MachEnableIntr)

LEAF(spl0)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	nop
	or	t0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
	mtc0	t0, MACH_COP_0_STATUS_REG	# enable all interrupts
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(spl0)

LEAF(splsoftclock)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~MACH_SOFT_INT_MASK_0	# disable soft clock
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(splsoftclock)

LEAF(splsoftnet)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(splsoftnet)

LEAF(Mach_spl0)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_0|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl0)

LEAF(Mach_spl1)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_1|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl1)

LEAF(Mach_spl2)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_2|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl2)

LEAF(Mach_spl3)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_3|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4x00
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl3)

LEAF(Mach_spl4)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_4|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl4)

LEAF(Mach_spl5)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~(MACH_INT_MASK_5|MACH_SOFT_INT_MASK_1|MACH_SOFT_INT_MASK_0)
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(Mach_spl5)


/*
 * We define an alternate entry point after mcount is called so it
 * can be used in mcount without causeing a recursive loop.
 */
LEAF(splhigh)
ALEAF(_splhigh)
	mfc0	v0, MACH_COP_0_STATUS_REG	# read status register
	li	t0, ~MIPS_SR_INT_IE	# disable all interrupts
	and	t0, t0, v0
	mtc0	t0, MACH_COP_0_STATUS_REG	# save it
	nop					# 3 ins to disable on r4000
	j	ra
	and	v0, v0, (MACH_INT_MASK | MIPS_SR_INT_IE)
END(splhigh)

/*
 * Restore saved interrupt mask.
 */
LEAF(splx)
ALEAF(_splx)
	mfc0	v0, MACH_COP_0_STATUS_REG
	li	t0, ~(MACH_INT_MASK | MIPS_SR_INT_IE)
	and	t0, t0, v0
	or	t0, t0, a0
	mtc0	t0, MACH_COP_0_STATUS_REG
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
 *		XXX  START of r3000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */



#if 0
/*----------------------------------------------------------------------------
 *
 * MachTLBModException --
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
NLEAF(MachTLBModException)
ALEAF(mips_r2000_TLBModException)
	.set	noat
	tlbp					# find the TLB entry
	mfc0	k0, MACH_COP_0_TLB_LOW		# get the physical address
	mfc0	k1, MACH_COP_0_TLB_INDEX	# check to be sure its valid
	or	k0, k0, VMMACH_TLB_MOD_BIT	# update TLB
	blt	k1, zero, 4f			# not found!!!
	mtc0	k0, MACH_COP_0_TLB_LOW
	li	k1, MACH_CACHED_MEMORY_ADDR
	subu	k0, k0, k1
	srl	k0, k0, VMMACH_TLB_PHYS_PAGE_SHIFT
	la	k1, pmap_attributes
	addu	k0, k0, k1
	lbu	k1, 0(k0)			# fetch old value
	nop
	or	k1, k1, 1			# set modified bit
	sb	k1, 0(k0)			# save new value
	mfc0	k0, MACH_COP_0_EXC_PC		# get return address
	nop
	j	k0
	rfe
4:
	break	0				# panic
	.set	at
END(MachTLBModException)
#endif

/*----------------------------------------------------------------------------
 *
 * MachTLBMissException --
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
NLEAF(MachTLBMissException)
ALEAF(mips_r2000_TLBMissException)
	.set	noat
	mfc0	k0, MACH_COP_0_BAD_VADDR	# get the fault address
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
	mfc0	k1, MACH_COP_0_EXC_PC		# get return address
	mtc0	k0, MACH_COP_0_TLB_LOW		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, _C_LABEL(MachKernGenException) # PTE invalid
	nop
	tlbwr					# update TLB
	j	k1
	rfe

1:
	subu	k0, sp, UADDR + 0x200		# check to see if we have a
	sltiu	k0, UPAGES*NBPG - 0x200		#  valid kernel stack
	bne	k0, zero, _C_LABEL(MachKernGenException) # Go panic
	nop

	la	a0, start - START_FRAME - 8	# set sp to a valid place
	sw	sp, 24(a0)
	move	sp, a0
	la	a0, 1f
	mfc0	a2, MACH_COP_0_STATUS_REG
	mfc0	a3, MACH_COP_0_CAUSE_REG
	mfc0	a1, MACH_COP_0_EXC_PC
	sw	a2, 16(sp)
	sw	a3, 20(sp)
	sw	sp, 24(sp)
	move	a2, ra
	jal	_C_LABEL(printf)
	mfc0	a3, MACH_COP_0_BAD_VADDR
	.data
1:
	.asciiz	"ktlbmiss: PC %x RA %x ADR %x\nSR %x CR %x SP %x\n"
	.text

	la	sp, start - START_FRAME		# set sp to a valid place
	PANIC("kernel stack overflow")
	.set	at
END(MachTLBMissException)


/*--------------------------------------------------------------------------
 *
 * MachTLBWriteIndexed --
 *
 *	Write the given entry into the TLB at the given index.
 *
 *	MachTLBWriteIndexed(index, highEntry, lowEntry)
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
LEAF(MachTLBWriteIndexed)
ALEAF(mips_r2000_TLBWriteIndexed)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Save the current PID.

	sll	a0, a0, VMMACH_TLB_INDEX_SHIFT
	mtc0	a0, MACH_COP_0_TLB_INDEX	# Set the index.
	mtc0	a1, MACH_COP_0_TLB_HI		# Set up entry high.
	mtc0	a2, MACH_COP_0_TLB_LOW		# Set up entry low.
	nop
	tlbwi					# Write the TLB

	mtc0	t0, MACH_COP_0_TLB_HI		# Restore the PID.
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBWriteIndexed)

#if 0
/*--------------------------------------------------------------------------
 *
 * MachTLBWriteRandom --
 *
 *	Write the given entry into the TLB at a random location.
 *
 *	MachTLBWriteRandom(highEntry, lowEntry)
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
LEAF(MachTLBWriteRandom)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	v0, MACH_COP_0_TLB_HI		# Save the current PID.
	nop

	mtc0	a0, MACH_COP_0_TLB_HI		# Set up entry high.
	mtc0	a1, MACH_COP_0_TLB_LOW		# Set up entry low.
	nop
	tlbwr					# Write the TLB

	mtc0	v0, MACH_COP_0_TLB_HI		# Restore the PID.
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBWriteRandom)
#endif

/*--------------------------------------------------------------------------
 *
 * MachSetPID --
 *
 *	Write the given pid into the TLB pid reg.
 *
 *	MachSetPID(pid)
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
LEAF(MachSetPID)
ALEAF(mips_r2000_SetPID)
	sll	a0, a0, VMMACH_TLB_PID_SHIFT	# put PID in right spot
	mtc0	a0, MACH_COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(MachSetPID)

/*--------------------------------------------------------------------------
 *
 * MachTLBFlush --
 *
 *	Flush the "random" entries from the TLB.
 *
 *	MachTLBFlush()
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(MachTLBFlush)
ALEAF(mips_r2000_TLBFlush)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Save the PID
	li	t1, MACH_CACHED_MEMORY_ADDR	# invalid address
	mtc0	t1, MACH_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MACH_COP_0_TLB_LOW	# Zero out low entry.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
	li	t1, VMMACH_FIRST_RAND_ENTRY << VMMACH_TLB_INDEX_SHIFT
	li	t2, VMMACH_NUM_TLB_ENTRIES << VMMACH_TLB_INDEX_SHIFT
1:
	mtc0	t1, MACH_COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, 1 << VMMACH_TLB_INDEX_SHIFT	# Increment index.
	bne	t1, t2, 1b
	tlbwi					# Write the TLB entry.

	mtc0	t0, MACH_COP_0_TLB_HI		# Restore the PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFlush)

#if 0
/*--------------------------------------------------------------------------
 *
 * MachTLBFlushPID --
 *
 *	Flush all entries with the given PID from the TLB.
 *
 *	MachTLBFlushPID(pid)
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
LEAF(MachTLBFlushPID)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Save the current PID
	sll	a0, a0, VMMACH_TLB_PID_SHIFT	# Align the pid to flush.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
	li	t1, VMMACH_FIRST_RAND_ENTRY << VMMACH_TLB_INDEX_SHIFT
	li	t2, VMMACH_NUM_TLB_ENTRIES << VMMACH_TLB_INDEX_SHIFT
	mtc0	t1, MACH_COP_0_TLB_INDEX	# Set the index register
1:
	addu	t1, t1, 1 << VMMACH_TLB_INDEX_SHIFT	# Increment index.
	tlbr					# Read from the TLB
	mfc0	t4, MACH_COP_0_TLB_HI		# Fetch the hi register.
	nop
	and	t4, t4, VMMACH_TLB_PID		# compare PIDs
	bne	t4, a0, 2f
	li	v0, MACH_CACHED_MEMORY_ADDR	# invalid address
	mtc0	v0, MACH_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MACH_COP_0_TLB_LOW	# Zero out low entry.
	nop
	tlbwi					# Write the entry.
2:
	bne	t1, t2, 1b
	mtc0	t1, MACH_COP_0_TLB_INDEX	# Set the index register

	mtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFlushPID)
#endif

/*--------------------------------------------------------------------------
 *
 * MachTLBFlushAddr --
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 *	MachTLBFlushAddr(highreg)
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
LEAF(MachTLBFlushAddr)
ALEAF(mips_r2000_TLBFlushAddr)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Get current PID
	nop

	mtc0	a0, MACH_COP_0_TLB_HI		# look for addr & PID
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MACH_COP_0_TLB_INDEX	# See what we got
	li	t1, MACH_CACHED_MEMORY_ADDR	# Load invalid entry.
	bltz	v0, 1f				# index < 0 => !found
	mtc0	t1, MACH_COP_0_TLB_HI		# Mark entry high as invalid
	mtc0	zero, MACH_COP_0_TLB_LOW	# Zero out low entry.
	nop
	tlbwi
1:
	mtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * MachTLBUpdate --
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 *	MachTLBUpdate(highreg, lowreg)
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
LEAF(MachTLBUpdate)
ALEAF(mips_r2000_TLBUpdate)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Save current PID
	nop					# 2 cycles before intr disabled
	mtc0	a0, MACH_COP_0_TLB_HI		# init high reg.
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MACH_COP_0_TLB_INDEX	# See what we got
	mtc0	a1, MACH_COP_0_TLB_LOW		# init low reg.
	bltz	v0, 1f				# index < 0 => !found
	sra	v0, v0, VMMACH_TLB_INDEX_SHIFT	# convert index to regular num
	b	2f
	tlbwi					# update slot found
1:
	mtc0	a0, MACH_COP_0_TLB_HI		# init high reg.
	nop
	tlbwr					# enter into a random slot
2:
	mtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBUpdate)

/*--------------------------------------------------------------------------
 *
 * MachTLBFind --
 *
 *	Search the TLB for the given entry.
 *
 *	MachTLBFind(hi)
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
LEAF(MachTLBFind)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Get current PID
	nop
	mtc0	a0, MACH_COP_0_TLB_HI		# Set up entry high.
	nop
	tlbp					# Probe for the entry.
	mfc0	v0, MACH_COP_0_TLB_INDEX	# See what we got
	nop
	bltz	v0, 1f				# not found
	nop
	tlbr					# read TLB
	mfc0	t1, MACH_COP_0_TLB_HI		# See what we got
	mfc0	t2, MACH_COP_0_TLB_LOW		# See what we got
	sw	t1, tlbhi
	sw	t2, tlblo
	srl	v0, v0, VMMACH_TLB_INDEX_SHIFT	# convert index to regular num
1:
	mtc0	t0, MACH_COP_0_TLB_HI		# Restore current PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFind)

/*--------------------------------------------------------------------------
 *
 * MachTLBRead --
 *
 *	Read the TLB entry.
 *
 *	MachTLBRead(entry)
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
LEAF(MachTLBRead)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t0, MACH_COP_0_TLB_HI		# Get current PID

	sll	a0, a0, VMMACH_TLB_INDEX_SHIFT
	mtc0	a0, MACH_COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr					# Read from the TLB
	mfc0	t3, MACH_COP_0_TLB_HI		# fetch the hi entry
	mfc0	t4, MACH_COP_0_TLB_LOW		# fetch the low entry
	sw	t3, tlbhi
	sw	t4, tlblo

	mtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBRead)

/*--------------------------------------------------------------------------
 *
 * MachTLBGetPID --
 *
 *	MachTLBGetPID()
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(MachTLBGetPID)
	mfc0	v0, MACH_COP_0_TLB_HI		# get PID
	nop
	and	v0, v0, VMMACH_TLB_PID		# mask off PID
	j	ra
	srl	v0, v0, VMMACH_TLB_PID_SHIFT	# put PID in right spot
END(MachTLBGetPID)


/*----------------------------------------------------------------------------
 *
 *	R3000 cache sizing and flushing code.
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 * MachConfigCache --
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
NON_LEAF(MachConfigCache, STAND_FRAME_SIZE, ra)
ALEAF(mips_r2000_ConfigCache)
	subu	sp, sp, STAND_FRAME_SIZE
	sw	ra, STAND_RA_OFFSET(sp)		# Save return address.
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts.
	la	v0, 1f
	or	v0, MACH_UNCACHED_MEMORY_ADDR	# Run uncached.
	j	v0
	nop
1:
/*
 * This works because jal doesn't change pc[31..28] and the
 * linker still thinks SizeCache is in the cached region so it computes
 * the correct address without complaining.
 */
	jal	_C_LABEL(SizeCache)		# Get the size of the d-cache.
	nop
	sw	v0, _C_LABEL(machDataCacheSize)
	nop					# Make sure sw out of pipe
	nop
	nop
	nop
	li	v0, MACH_SR_SWAP_CACHES		# Swap caches
	mtc0	v0, MACH_COP_0_STATUS_REG
	nop					# Insure caches stable
	nop
	nop
	nop
	jal	_C_LABEL(SizeCache)		# Get the size of the i-cache.
	nop
	mtc0	zero, MACH_COP_0_STATUS_REG	# Swap back caches and enable.
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
END(MachConfigCache)

/*----------------------------------------------------------------------------
 *
 * SizeCache --
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
LEAF(SizeCache)
ALEAF(mips_r2000_SizeCache)
	mfc0	t0, MACH_COP_0_STATUS_REG	# Save the current status reg.
	nop
	or	v0, t0, MACH_SR_ISOL_CACHES	# Isolate the caches.
	nop					# Make sure no stores in pipe
	mtc0	v0, MACH_COP_0_STATUS_REG
	nop					# Make sure isolated
	nop
	nop
/*
 * Clear cache size boundaries.
 */
	li	v0, MACH_MIN_CACHE_SIZE
	li	v1, MACH_CACHED_MEMORY_ADDR
	li	t2, MACH_MAX_CACHE_SIZE
1:
	addu	t1, v0, v1			# Compute address to clear
	sw	zero, 0(t1)			# Clear cache memory
	bne	v0, t2, 1b
	sll	v0, v0, 1

	li	v0, -1
	sw	v0, 0(v1)			# Store marker in cache
	li	v0, MACH_MIN_CACHE_SIZE
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
	mtc0	t0, MACH_COP_0_STATUS_REG
	nop					# Make sure unisolated
	nop
	nop
	nop
	j	ra
	nop
END(SizeCache)

/*----------------------------------------------------------------------------
 *
 * MachFlushCache --
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
LEAF(MachFlushCache)
ALEAF(mips_r2000_FlushCache)
	lw	t1, _C_LABEL(machInstCacheSize)	# Must load before isolating
	lw	t2, _C_LABEL(machDataCacheSize)	# Must load before isolating
	mfc0	t3, MACH_COP_0_STATUS_REG 	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts.
	la	v0, 1f
	or	v0, MACH_UNCACHED_MEMORY_ADDR	# Run uncached.
	j	v0
	nop
/*
 * Flush the instruction cache.
 */
1:
	li	v0, MACH_SR_ISOL_CACHES | MACH_SR_SWAP_CACHES
	mtc0	v0, MACH_COP_0_STATUS_REG	# Isolate and swap caches.
	li	t0, MACH_UNCACHED_MEMORY_ADDR
	subu	t0, t0, t1
	li	t1, MACH_UNCACHED_MEMORY_ADDR
	la	v0, 1f				# Run cached
	j	v0
	nop
1:
	addu	t0, t0, 4
	bne	t0, t1, 1b
	sb	zero, -4(t0)

	la	v0, 1f
	or	v0, MACH_UNCACHED_MEMORY_ADDR
	j	v0				# Run uncached
	nop
/*
 * Flush the data cache.
 */
1:
	li	v0, MACH_SR_ISOL_CACHES
	mtc0	v0, MACH_COP_0_STATUS_REG	# Isolate and swap back caches
	li	t0, MACH_UNCACHED_MEMORY_ADDR
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
	mtc0	t3, MACH_COP_0_STATUS_REG	# Restore status reg.
	nop					# Insure cache unisolated.
	nop
	nop
	nop
	j	ra
	nop
END(MachFlushCache)

/*----------------------------------------------------------------------------
 *
 * MachFlushICache --
 *
 *	void MachFlushICache(addr, len)
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
LEAF(MachFlushICache)
ALEAF(mips_r2000_FlushICache)
	mfc0	t0, MACH_COP_0_STATUS_REG	# Save SR
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts.

	la	v1, 1f
	or	v1, MACH_UNCACHED_MEMORY_ADDR	# Run uncached.
	j	v1
	nop
1:
	bc0f	1b				# make sure stores are complete
	li	v1, MACH_SR_ISOL_CACHES | MACH_SR_SWAP_CACHES
	mtc0	v1, MACH_COP_0_STATUS_REG
	nop
	addu	a1, a1, a0			# compute ending address
1:
	addu	a0, a0, 4
	bne	a0, a1, 1b
	sb	zero, -4(a0)

	mtc0	t0, MACH_COP_0_STATUS_REG	# enable interrupts
	j	ra				# return and run cached
	nop
END(MachFlushICache)

/*----------------------------------------------------------------------------
 *
 * MachFlushDCache --
 *
 *	void MachFlushDCache(addr, len)
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
LEAF(MachFlushDCache)
ALEAF(mips_r2000_FlushDCache)
	mfc0	t0, MACH_COP_0_STATUS_REG	# Save SR
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts.
	nop
1:
	bc0f	1b				# make sure stores are complete
# BUG: should drain write buffer.
# The insn above does not work on some all DEC machines, or all variants
# of the mips architecture.
	li	v1, MACH_SR_ISOL_CACHES
	mtc0	v1, MACH_COP_0_STATUS_REG
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
	mtc0	t0, MACH_COP_0_STATUS_REG	# enable interrupts
	nop
	j	ra				# return and run cached
	nop
END(MachFlushDCache)

/*----------------------------------------------------------------------------
 *
 *		XXX  END of r3000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */



#ifdef notyet	/* XXX -- r4000 support, not yet */


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
 * MachTLBMInvalidException --
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
NLEAF(MachTLBInvalidException)
	.set	noat
	dmfc0	k0, MACH_COP_0_BAD_VADDR	# get the fault address
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

	mfc0	k0, MACH_COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, MACH_COP_0_TLB_LO0		# load PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, MachKernGenException	# PTE invalid
	lw	k0, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MACH_COP_0_TLB_LO1		# load PTE entry
	nop
	tlbwi					# write TLB
	nop
	nop
	nop
	nop
	nop
	eret

KernTLBIOdd:
	mfc0	k0, MACH_COP_0_TLB_INDEX
	nop
	bltz	k0, sys_stk_chk
	sltiu	k0, k0, 8

	bne	k0, zero, sys_stk_chk
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of wired bit
	dsrl	k0, k0, 34
	dmtc0	k0, MACH_COP_0_TLB_LO1		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, MachKernGenException	# PTE invalid
	lw	k0, -4(k1)			# get even PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, MACH_COP_0_TLB_LO0		# save PTE entry
	nop
	tlbwi					# update TLB
	nop
	nop
	nop
	nop
	nop
	eret
END(MachTLBInvalidException)

/*----------------------------------------------------------------------------
 *
 * MachTLBMissException --
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
NLEAF(MachTLBMissException)
	.set	noat
	dmfc0	k0, MACH_COP_0_BAD_VADDR	# get the fault address
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
	dmtc0	k0, MACH_COP_0_TLB_LO0		# load PTE entry
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, MACH_COP_0_TLB_LO1		# load PTE entry
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
	bne	k0, zero, MachKernGenException	# Go panic
	nop

	la	a0, start - START_FRAME - 8	# set sp to a valid place
	sw	sp, 24(a0)
	move	sp, a0
	la	a0, 1f
	mfc0	a2, MACH_COP_0_STATUS_REG
	mfc0	a3, MACH_COP_0_CAUSE_REG
	dmfc0	a1, MACH_COP_0_EXC_PC
	sw	a2, 16(sp)
	sw	a3, 20(sp)
	move	a2, ra
	jal	printf
	dmfc0	a3, MACH_COP_0_BAD_VADDR
	.data
1:
	.asciiz	"ktlbmiss: PC %x RA %x ADR %x\nSR %x CR %x SP %x\n"
	.text

	la	sp, start - START_FRAME		# set sp to a valid place
	PANIC("kernel stack overflow")
	.set	at
END(MachTLBMissException)


/*--------------------------------------------------------------------------
 *
 * MachTLBWriteIndexed --
 *
 *	Write the given entry into the TLB at the given index.
 *
 *	MachTLBWriteIndexed(index, tlb)
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
LEAF(MachTLBWriteIndexed)
ALEAF(mips_r2000_TLBWriteIndexed)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	nop
	lw	a2, 8(a1)
	lw	a3, 12(a1)
	dmfc0	t0, MACH_COP_0_TLB_HI		# Save the current PID.

	dmtc0	a2, MACH_COP_0_TLB_LO0		# Set up entry low0.
	dmtc0	a3, MACH_COP_0_TLB_LO1		# Set up entry low1.
	lw	a2, 0(a1)
	lw	a3, 4(a1)
	mtc0	a0, MACH_COP_0_TLB_INDEX	# Set the index.
	dmtc0	a2, MACH_COP_0_TLB_PG_MASK	# Set up entry mask.
	dmtc0	a3, MACH_COP_0_TLB_HI		# Set up entry high.
	nop
	tlbwi					# Write the TLB
	nop
	nop
	nop					# Delay for effect
	nop

	dmtc0	t0, MACH_COP_0_TLB_HI		# Restore the PID.
	nop
	dmtc0	zero, MACH_COP_0_TLB_PG_MASK	# Default mask value.
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBWriteIndexed)

/*--------------------------------------------------------------------------
 *
 * MachSetPID --
 *
 *	Write the given pid into the TLB pid reg.
 *
 *	MachSetPID(pid)
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
LEAF(MachSetPID)
	dmtc0	a0, MACH_COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(MachSetPID)

/*--------------------------------------------------------------------------
 *
 * MachSetWIRED --
 *
 *	Write the given value into the TLB wired reg.
 *
 *	MachSetPID(wired)
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
LEAF(MachSetWIRED)
	mtc0	a0, MACH_COP_0_TLB_WIRED
	j	ra
	nop
END(MachSetWIRED)

/*--------------------------------------------------------------------------
 *
 * MachGetWIRED --
 *
 *	Get the value from the TLB wired reg.
 *
 *	MachGetWIRED(void)
 *
 * Results:
 *	Value of wired reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(MachGetWIRED)
	mfc0	v0, MACH_COP_0_TLB_WIRED
	j	ra
	nop
END(MachGetWIRED)

/*--------------------------------------------------------------------------
 *
 * MachTLBFlush --
 *
 *	Flush the "random" entries from the TLB.
 *	Uses "wired" register to determine what register to start with.
 *
 *	MachTLBFlush()
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(MachTLBFlush)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	mfc0	t1, MACH_COP_0_TLB_WIRED
	li	t2, VMMACH_NUM_TLB_ENTRIES
	li	v0, MACH_CACHED_MEMORY_ADDR	# invalid address
	dmfc0	t0, MACH_COP_0_TLB_HI		# Save the PID

	dmtc0	v0, MACH_COP_0_TLB_HI		# Mark entry high as invalid
	dmtc0	zero, MACH_COP_0_TLB_LO0	# Zero out low entry0.
	dmtc0	zero, MACH_COP_0_TLB_LO1	# Zero out low entry1.
	mtc0	zero, MACH_COP_0_TLB_PG_MASK	# Zero out mask entry.
/*
 * Align the starting value (t1) and the upper bound (t2).
 */
1:
	mtc0	t1, MACH_COP_0_TLB_INDEX	# Set the index register.
	addu	t1, t1, 1			# Increment index.
	tlbwi					# Write the TLB entry.
	nop
	nop
	bne	t1, t2, 1b
	nop

	dmtc0	t0, MACH_COP_0_TLB_HI		# Restore the PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFlush)


/*--------------------------------------------------------------------------
 *
 * MachTLBFlushAddr --
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 *	MachTLBFlushAddr(TLBhi)
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
LEAF(MachTLBFlushAddr)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	nop
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0			# Make shure valid hi value.
	dmfc0	t0, MACH_COP_0_TLB_HI		# Get current PID
	dmtc0	a0, MACH_COP_0_TLB_HI		# look for addr & PID
	nop
	nop
	nop
	tlbp					# Probe for the entry.
	nop
	nop					# Delay for effect
	nop
	mfc0	v0, MACH_COP_0_TLB_INDEX	# See what we got
	li	t1, MACH_CACHED_MEMORY_ADDR	# Load invalid entry.
	bltz	v0, 1f				# index < 0 => !found
	nop
	dmtc0	t1, MACH_COP_0_TLB_HI		# Mark entry high as invalid

	dmtc0	zero, MACH_COP_0_TLB_LO0	# Zero out low entry.
	dmtc0	zero, MACH_COP_0_TLB_LO1	# Zero out low entry.
	nop
	tlbwi
	nop
	nop
	nop
	nop
1:
	dmtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * MachTLBUpdate --
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 *	MachTLBUpdate(virpageadr, lowregx)
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
LEAF(MachTLBUpdate)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	and	t1, a0, 0x1000			# t1 = Even/Odd flag
	li	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0
	dmfc0	t0, MACH_COP_0_TLB_HI		# Save current PID
	dmtc0	a0, MACH_COP_0_TLB_HI		# Init high reg
	and	a2, a1, PG_G			# Copy global bit
	nop
	nop
	tlbp					# Probe for the entry.
	dsll	a1, a1, 34
	dsrl	a1, a1, 34
	bne	t1, zero, 2f			# Decide even odd
	mfc0	v0, MACH_COP_0_TLB_INDEX	# See what we got
# EVEN
	nop
	bltz	v0, 1f				# index < 0 => !found
	nop

	tlbr					# update, read entry first
	nop
	nop
	nop
	dmtc0	a1, MACH_COP_0_TLB_LO0		# init low reg0.
	nop
	tlbwi					# update slot found
	b	4f
	nop
1:
	mtc0	zero, MACH_COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, MACH_COP_0_TLB_HI		# init high reg.
	dmtc0	a1, MACH_COP_0_TLB_LO0		# init low reg0.
	dmtc0	a2, MACH_COP_0_TLB_LO1		# init low reg1.
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
	dmtc0	a1, MACH_COP_0_TLB_LO1		# init low reg1.
	nop
	tlbwi					# update slot found
	b	4f
	nop
3:
	mtc0	zero, MACH_COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, MACH_COP_0_TLB_HI		# init high reg.
	dmtc0	a2, MACH_COP_0_TLB_LO0		# init low reg0.
	dmtc0	a1, MACH_COP_0_TLB_LO1		# init low reg1.
	nop
	tlbwr					# enter into a random slot

4:						# Make shure pipeline
	nop					# advances before we
	nop					# uses the tlb.
	nop
	nop
	dmtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	j	ra
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
END(MachTLBUpdate)

/*--------------------------------------------------------------------------
 *
 * MachTLBRead --
 *
 *	Read the TLB entry.
 *
 *	MachTLBRead(entry, tlb)
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
LEAF(MachTLBRead)
	mfc0	v1, MACH_COP_0_STATUS_REG	# Save the status register.
	mtc0	zero, MACH_COP_0_STATUS_REG	# Disable interrupts
	nop
	nop
	nop
	dmfc0	t0, MACH_COP_0_TLB_HI		# Get current PID

	mtc0	a0, MACH_COP_0_TLB_INDEX	# Set the index register
	nop
	tlbr					# Read from the TLB
	nop
	nop
	nop
	mfc0	t2, MACH_COP_0_TLB_PG_MASK	# fetch the hi entry
	dmfc0	t3, MACH_COP_0_TLB_HI		# fetch the hi entry
	dmfc0	t4, MACH_COP_0_TLB_LO0		# See what we got
	dmfc0	t5, MACH_COP_0_TLB_LO1		# See what we got
	dmtc0	t0, MACH_COP_0_TLB_HI		# restore PID
	nop
	nop
	nop					# wait for PID active
	mtc0	v1, MACH_COP_0_STATUS_REG	# Restore the status register
	sw	t2, 0(a1)
	sw	t3, 4(a1)
	sw	t4, 8(a1)
	j	ra
	sw	t5, 12(a1)
END(MachTLBRead)

/*--------------------------------------------------------------------------
 *
 * MachTLBGetPID --
 *
 *	MachTLBGetPID()
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(MachTLBGetPID)
	dmfc0	v0, MACH_COP_0_TLB_HI		# get PID
	j	ra
	and	v0, v0, VMMACH_TLB_PID		# mask off PID
END(MachTLBGetPID)



/*----------------------------------------------------------------------------
 *
 *	R4000 cache sizing and flushing code.
 *
 *----------------------------------------------------------------------------
 */


/*----------------------------------------------------------------------------
 *
 * MachConfigCache --
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
LEAF(MachConfigCache)
	mfc0	v0, MACH_COP_0_CONFIG		# Get configuration register
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
END(MachConfigCache)

/*----------------------------------------------------------------------------
 *
 * MachFlushCache --
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
LEAF(MachFlushCache)
	lw	t1, machPrimaryInstCacheSize
	lw	t2, machPrimaryDataCacheSize
 #	lw	t3, machPrimaryInstCacheLSize
 #	lw	t4, machPrimaryDataCacheLSize
/*
 * Flush the instruction cache.
 */
	li	t0, MACH_CACHED_MEMORY_ADDR
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
	li	t0, MACH_CACHED_MEMORY_ADDR
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
END(MachFlushCache)

/*----------------------------------------------------------------------------
 *
 * MachFlushICache --
 *
 *	void MachFlushICache(addr, len)
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
LEAF(MachFlushICache)
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
END(MachFlushICache)

/*----------------------------------------------------------------------------
 *
 * MachFlushDCache --
 *
 *	void MachFlushDCache(addr, len)
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
LEAF(MachFlushDCache)
ALEAF(mips_r2000_FlushDCache)
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
END(MachFlushDCache)

/*----------------------------------------------------------------------------
 *
 * MachHitFlushDCache --
 *
 *	void MachHitFlushDCache(addr, len)
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
LEAF(MachHitFlushDCache)
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
END(MachHitFlushDCache)
/*----------------------------------------------------------------------------
 *
 * MachInvalidateDCache --
 *
 *	void MachFlushDCache(addr, len)
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
LEAF(MachInvalidateDCache)
	addu	a1, a1, a0			# compute ending address
1:
	addu	a0, a0, 4
	bne	a0, a1, 1b
	cache	0x11,-4(a0)

	j	ra
	nop
END(MachInvalidateDCache)

/*----------------------------------------------------------------------------
 *
 *			XXX  END of r4000-specific code  XXX 
 *
 *----------------------------------------------------------------------------
 */

#endif 	/* XXX -- r4000 support, not yet */


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
	mfc0	v0, MACH_COP_0_CAUSE_REG
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
	mfc0	t1, MACH_COP_0_STATUS_REG	# Save old SR
	li	t0, MACH_SR_COP_1_BIT		# enable the coprocessor
	mtc0	t0, MACH_COP_0_STATUS_REG

	beq	a0, zero, 1f			# skip save if NULL pointer
	nop
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	a0, P_ADDR(a0)			# get pointer to pcb for proc
	cfc1	t0, MACH_FPC_CSR		# stall til FP done
	cfc1	t0, MACH_FPC_CSR		# now get status
	li	t3, ~MACH_SR_COP_1_BIT
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

	and	t0, t0, ~MACH_FPC_EXCEPTION_BITS
	ctc1	t0, MACH_FPC_CSR
	nop

	mtc0	t1, MACH_COP_0_STATUS_REG	# Restore the status register.
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
	mfc0	t1, MACH_COP_0_STATUS_REG	# Disable interrupts and
	li	t0, MACH_SR_COP_1_BIT		#  enable the coprocessor
	mtc0	t0, MACH_COP_0_STATUS_REG
	sw	zero, _C_LABEL(machFPCurProcPtr) # indicate state has been saved
/*
 * First read out the status register to make sure that all FP operations
 * have completed.
 */
	lw	t2, U_PCB_REGS+(PS * 4)(a0)	# get CPU status register
	li	t3, ~MACH_SR_COP_1_BIT
	and	t2, t2, t3			# clear COP_1 enable bit
	cfc1	t0, MACH_FPC_CSR		# stall til FP done
	cfc1	t0, MACH_FPC_CSR		# now get status
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

	mtc0	t1, MACH_COP_0_STATUS_REG	# Restore the status register.
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
	mfc0	t0, MACH_COP_0_STATUS_REG
	sw	ra, STAND_RA_OFFSET(sp)
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)

	or	t1, t0, MACH_SR_COP_1_BIT
	mtc0	t1, MACH_COP_0_STATUS_REG
	nop
	nop
	nop				# 1st extra nop for r4k
	nop				# 2nd extra nop for r4k

	cfc1	t1, MACH_FPC_CSR	# stall til FP done
	cfc1	t1, MACH_FPC_CSR	# now get status
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
	srl	a3, a0, MACH_OPCODE_SHIFT
	beq	a3, MACH_OPCODE_C1, 4f		# this should never fail
	nop
/*
 * Send a floating point exception signal to the current process.
 */
3:
	lw	a0, _C_LABEL(curproc)		# get current process
	cfc1	a2, MACH_FPC_CSR		# code = FP execptions
	ctc1	zero, MACH_FPC_CSR		# Clear exceptions
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
	mfc0	t0, MACH_COP_0_STATUS_REG
	lw	ra, STAND_RA_OFFSET(sp)
	and	t0, t0, ~MACH_SR_COP_1_BIT
	mtc0	t0, MACH_COP_0_STATUS_REG
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
	b	MachFlushICache		# flush instruction cache
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
	break	MACH_BREAK_KDB_VAL
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
 * Interrupt counters for vmstat.
 */
	.data
	.globl _C_LABEL(intrcnt)
	.globl _C_LABEL(eintrcnt)
	.globl _C_LABEL(intrnames)
	.globl _C_LABEL(eintrnames)
_C_LABEL(intrnames):
	.asciiz	"softclock"
	.asciiz	"softnet"
	.asciiz	"serial0"
	.asciiz	"serial1"
	.asciiz	"serial2"
	.asciiz	"ether"
	.asciiz	"scsi"
	.asciiz	"memory"
	.asciiz	"clock"
	.asciiz	"fpu"
	.asciiz	"tcslot0"
	.asciiz	"tcslot1"
	.asciiz	"tcslot2"
	.asciiz "dtop"
	.asciiz "isdn"
	.asciiz	"floppy"
	.asciiz	"stray"
	.asciiz	"nmi"
	.asciiz	"lostclock"
_C_LABEL(eintrnames):
	.align	2
_C_LABEL(intrcnt):
	.word	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0
_C_LABEL(eintrcnt):
	.word	0	# This shouldn't be needed but with 4.4bsd's as, the eintrcnt
			#  label ends end up in a different section otherwise.
