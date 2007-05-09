/*	$OpenBSD: context.S,v 1.11 2007/05/09 19:19:26 miod Exp $ */

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

#define curproc	(cpu_info_primary + CI_CURPROC)

	.set	mips3

	.set	noreorder		# Noreorder is default style!

/*
 * Save registers and state used by reboot to take snapshot.
 */
LEAF(savectx, 0)
	REG_S	s0, PCB_CONTEXT+0*REGSZ(a0)
	REG_S	s1, PCB_CONTEXT+1*REGSZ(a0)
	REG_S	s2, PCB_CONTEXT+2*REGSZ(a0)
	REG_S	s3, PCB_CONTEXT+3*REGSZ(a0)
	mfc0	v0, COP_0_STATUS_REG
	REG_S	s4, PCB_CONTEXT+4*REGSZ(a0)
	REG_S	s5, PCB_CONTEXT+5*REGSZ(a0)
	REG_S	s6, PCB_CONTEXT+6*REGSZ(a0)
	REG_S	s7, PCB_CONTEXT+7*REGSZ(a0)
	REG_S	sp, PCB_CONTEXT+8*REGSZ(a0)
	REG_S	s8, PCB_CONTEXT+9*REGSZ(a0)
	REG_S	ra, PCB_CONTEXT+10*REGSZ(a0)
	REG_S	v0, PCB_CONTEXT+11*REGSZ(a0)
	cfc0	t1, COP_0_ICR
	lw	t0, cpl
	REG_S	t1, PCB_CONTEXT+12*REGSZ(a0)	# save status register
	REG_S	t0, PCB_CONTEXT+13*REGSZ(a0)
	j	ra
	move	v0, zero
END(savectx)

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
NON_LEAF(setrunqueue, FRAMESZ(CF_SZ), ra)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_L	t0, P_BACK(a0)		## firewall: p->p_back must be 0
	bne	t0, zero, 1f		##
	lbu	t0, P_PRIORITY(a0)	# put on p->p_priority / 4 queue
	li	t1, 1			# compute corresponding bit
	srl	t0, t0, 2		# compute index into 'whichqs'
	sll	t1, t1, t0
	lw	t2, whichqs		# set corresponding bit
	sll	t0, t0, LOGREGSZ+1	# compute index into 'qs'
	or	t2, t2, t1
	sw	t2, whichqs
	LA	t1, qs
	PTR_ADDU t0, t0, t1		# t0 = qp = &qs[pri >> 2]
	PTR_L	t1, P_BACK(t0)		# t1 = qp->ph_rlink
	PTR_S	t0, P_FORW(a0)		# p->p_forw = qp
	PTR_S	t1, P_BACK(a0)		# p->p_back = qp->ph_rlink
	PTR_S	a0, P_FORW(t1)		# p->p_back->p_forw = p;
	j	ra
	PTR_S	a0, P_BACK(t0)		# qp->ph_rlink = p

1:
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	PTR_S	ra, CF_RA_OFFS(sp)
	PANIC("setrunqueue")
	jr	ra
	nop
END(setrunqueue)

/*
 * Remrq(p)
 *
 * Call should be made at splclock().
 */
NON_LEAF(remrunqueue, FRAMESZ(CF_SZ), ra)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	lbu	t0, P_PRIORITY(a0)	# get from p->p_priority / 4 queue
	li	t1, 1			# compute corresponding bit
	srl	t0, t0, 2		# compute index into 'whichqs'
	lw	t2, whichqs		# check corresponding bit
	sll	t1, t1, t0
	and	v0, t2, t1
	beqz	v0, 2f			# oops! queue is empty!
	PTR_L	v0, P_BACK(a0)		# v0 = p->p_back

	PTR_L	v1, P_FORW(a0)		# v1 = p->p_forw
	PTR_SLL	t0, t0, LOGREGSZ+1	# compute index into 'qs'
	PTR_S	v1, P_FORW(v0)		# p->p_back->p_forw = p->p_forw;
	PTR_S	v0, P_BACK(v1)		# p->p_forw->p_back = p->r_rlink
	LA	v0, qs
	PTR_ADDU t0, t0, v0		# t0 = qp = &qs[pri >> 2]
	PTR_L	v0, P_FORW(t0)		# check if queue empty
	bne	v0, t0, 1f		# No. qp->ph_link != qp
	xor	t2, t2, t1		# clear corresponding bit in 'whichqs'
	sw	t2, whichqs
1:
	j	ra
	PTR_S	zero, P_BACK(a0)	# for firewall checking

2:
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	PTR_S	ra, CF_RA_OFFS(sp)
	PANIC("remrunqueue empty")
	jr	ra
	nop
END(remrunqueue)

/*
 *   Idle, this is where we spend time when nothing to do.
 */
LEAF(idle, 0)
_idle:
	PTR_S	zero, curproc		# set curproc NULL for stats
	sw	zero, cpl		# lower to spl0
	lw	t0, ipending
	beqz	t0, 1f
	nop
	jal	setsoftintr0
	nop
1:
	mfc0	a0, COP_0_STATUS_REG
	li	a1, SR_INT_ENAB
	or	a0, a0, a1
	mtc0	a0, COP_0_STATUS_REG
	ITLBNOPFIX

#ifdef IMASK_EXTERNAL
	jal	hw_setintrmask
	xor	a0, a0
#endif
	jal	updateimask		# Make sure SR imask is updated
	xor	a0, a0

	li	t1,1
#if defined(TGT_CP7000) || defined(TGT_CP7000G)
	PTR_L	t2, misc_h		# if non zero, do Ocelot LEDs.
	beqz	t2, 1f
	li	t0, 0x40
	sb	t0, 0x0d(t2)
#endif
1:
	beq	t1, zero, 2f		# check if stuck in idle!
	addu	t1, t1, 1
	lw	t0, whichqs		# look for non-empty queue
	beq	t0, zero, 1b
	nop
#if defined(TGT_CP7000) || defined(TGT_CP7000G)
	beqz	t2, sw1
	li	t0, 0x40
	sb	t0, 0x0c(t2)
#endif
	b	sw1			# Hey, time to do some work!
	nop
2:
	break   BREAK_SOVER_VAL		# interrupt stuck!?
	b	1b
	nop
	jr	ra			# DDB trace
	nop
	.globl e_idle
e_idle:
END(idle)

/*
 * switch_exit(p)
 *
 * At exit of a process, do a cpu_switch for the last time.
 * All interrupts should be blocked at this point.
 */
LEAF(switch_exit, 0)
	mfc0	v0, COP_0_STATUS_REG
	li	v1, ~SR_INT_ENAB
	and	v0, v0, v1
	mtc0	v0, COP_0_STATUS_REG
	ITLBNOPFIX
	LA	sp, idle_stack - FRAMESZ(CF_SZ)
	jal	exit2
	nop

	PTR_S	zero, curproc
END(switch_exit)
	/* FALL THROUGH TO cpu switch! */
/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
NON_LEAF(cpu_switch, FRAMESZ(CF_SZ), ra)
	PTR_L	t3, curprocpaddr
	REG_S	sp, PCB_CONTEXT+8*REGSZ(t3)	# save old sp
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	REG_S	ra, CF_RA_OFFS(sp)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	lw	t0, cpl
	sw	t0, PCB_CONTEXT+13*REGSZ(t3)
#	lw	t2, cnt+V_SWTCH			# for statistics
	REG_S	s0, PCB_CONTEXT+0*REGSZ(t3)	# do a 'savectx()'
	REG_S	s1, PCB_CONTEXT+1*REGSZ(t3)
	REG_S	s2, PCB_CONTEXT+2*REGSZ(t3)
	REG_S	s3, PCB_CONTEXT+3*REGSZ(t3)
	REG_S	s4, PCB_CONTEXT+4*REGSZ(t3)
	REG_S	s5, PCB_CONTEXT+5*REGSZ(t3)
	REG_S	s6, PCB_CONTEXT+6*REGSZ(t3)
	REG_S	s7, PCB_CONTEXT+7*REGSZ(t3)
	REG_S	s8, PCB_CONTEXT+9*REGSZ(t3)
	REG_S	ra, PCB_CONTEXT+10*REGSZ(t3)
	mfc0	t0, COP_0_STATUS_REG
	cfc0	t1, COP_0_ICR
	REG_S	t0, PCB_CONTEXT+11*REGSZ(t3)
	REG_S	t1, PCB_CONTEXT+12*REGSZ(t3)

	lw	t1, whichqs			# look for non-empty queue
#	addu	t2, t2, 1
#	sw	t2, cnt+V_SWTCH
	beq	t1, zero, _idle			# if none, idle
	nop
sw1:
	mfc0	v0, COP_0_STATUS_REG
	li	v1, ~SR_INT_ENAB
	and	v0, v0, v1
	mtc0	v0, COP_0_STATUS_REG
	ITLBNOPFIX
	lw	t0, whichqs			# look for non-empty queue
	li	t2, -1				# t2 = lowest bit set
	beq	t0, zero, _idle			# if none, idle
	move	t3, t0				# t3 = saved whichqs
1:
	addu	t2, t2, 1
	and	t1, t0, 1			# bit set?
	beq	t1, zero, 1b
	srl	t0, t0, 1			# try next bit
/*
 * Remove process from queue.
 */
	PTR_SLL	t0, t2, LOGREGSZ+1
	LA	t1, qs
	PTR_ADDU t0, t0, t1			# t0 = qp = &qs[highbit]
	PTR_L	a0, P_FORW(t0)			# a0 = p = highest pri process
	PTR_L	v0, P_FORW(a0)			# v0 = p->p_forw
	beq	t0, a0, 4f			# make sure something in queue
	PTR_S	v0, P_FORW(t0)			# qp->ph_link = p->p_forw;
	PTR_S	t0, P_BACK(v0)			# p->p_forw->p_back = qp
	bne	v0, t0, 2f			# queue still not empty
	PTR_S	zero, P_BACK(a0)		## for firewall checking
	li	v1, 1				# compute bit in 'whichqs'
	sll	v1, v1, t2
	xor	t3, t3, v1			# clear bit in 'whichqs'
	sw	t3, whichqs
2:
/*
 * Switch to new context.
 */
	sw	zero, want_resched
	jal	pmap_activate			# v0 = TLB PID
	move	s0, a0				# BDSLOT: save p

/*
 * We need to wire the process kernel stack mapping so there
 * will be no tlb misses in exception handlers. This is done
 * by invalidating any tlb entries mapping the U-area and
 * put valid mappings in tlb entries 0 and 1.
 */

	PTR_L	t3, P_ADDR(s0)			# get uarea pointer.
	PTR_S	s0, curproc			# set curproc
	PTR_S	t3, curprocpaddr

	li	t1, SONPROC
	sb	t1, P_STAT(s0)			# set to onproc.

	or	v0, t3
	dmtc0	v0, COP_0_TLB_HI		# init high entry (tlbid)
	LA	t1, (VM_MIN_KERNEL_ADDRESS)
	PTR_SUBU t2, t3, t1
	bltz	t2, ctx3			# not mapped.
	PTR_SRL	t2, PGSHIFT+1
	PTR_L	t1, Sysmap
	tlbp
	PTR_SLL	t2, 3
	PTR_ADDU t1, t2				# t1 now points at ptes.
	mfc0	t0, COP_0_TLB_INDEX
	nop
	bltz	t0, ctx1			# not in tlb
	LA	t2, KSEG0_BASE		# NOTE: if > 1 ins, does not matter

	dmtc0	t2, COP_0_TLB_HI		# invalidate it.
	dmtc0	zero, COP_0_TLB_LO0
	dmtc0	zero, COP_0_TLB_LO1
	nop
	nop
	nop
	nop
	tlbwi
	nop
	nop
	nop

ctx1:
	mtc0	zero, COP_0_TLB_INDEX
	dmtc0	v0, COP_0_TLB_HI
	lw	ta0, 0(t1)
	lw	ta1, 4(t1)
	dsll	ta0, ta0, 34
	dsrl	ta0, ta0, 34
	dsll	ta1, ta1, 34
	dsrl	ta1, ta1, 34
	dmtc0	ta0, COP_0_TLB_LO0
	dmtc0	ta1, COP_0_TLB_LO1
	nop
	PTR_ADDU v0, 2*NBPG
	nop
	nop
	tlbwi

#if (UPAGES != 2)
	dmtc0	v0, COP_0_TLB_HI		# init high entry (tlbid)
	lw	ta0, 8(t1)
	lw	ta1, 12(t1)
	dsll	ta0, ta0, 34
	dsrl	ta0, ta0, 34
	tlbp
	nop
	dsll	ta1, ta1, 34
	dsrl	ta1, ta1, 34
	mfc0	t0, COP_0_TLB_INDEX
	nop
	bltz	t0, ctx2			# not in tlb
	li	t2, 1

	dmtc0	t2, COP_0_TLB_HI		# invalidate it.
	dmtc0	zero, COP_0_TLB_LO0
	dmtc0	zero, COP_0_TLB_LO1
	nop
	nop
	nop
	nop
	tlbwi
	nop
	nop
	nop

ctx2:
	mtc0	t2, COP_0_TLB_INDEX
	dmtc0	v0, COP_0_TLB_HI
	dmtc0	ta0, COP_0_TLB_LO0
	dmtc0	ta1, COP_0_TLB_LO1
	nop
	nop
	nop
	nop
	tlbwi
#endif
	nop
	nop
	nop
	nop

ctx3:

/*
 * Restore registers and return.
 */
	REG_L	a0, PCB_CONTEXT+13*REGSZ(t3)
	REG_L	s0, PCB_CONTEXT+0*REGSZ(t3)
	REG_L	s1, PCB_CONTEXT+1*REGSZ(t3)
	REG_L	s2, PCB_CONTEXT+2*REGSZ(t3)
	REG_L	s3, PCB_CONTEXT+3*REGSZ(t3)
	REG_L	s4, PCB_CONTEXT+4*REGSZ(t3)
	REG_L	s5, PCB_CONTEXT+5*REGSZ(t3)
	REG_L	s6, PCB_CONTEXT+6*REGSZ(t3)
	REG_L	s7, PCB_CONTEXT+7*REGSZ(t3)
	REG_L	sp, PCB_CONTEXT+8*REGSZ(t3)
	REG_L	s8, PCB_CONTEXT+9*REGSZ(t3)
	sw	a0, cpl
#ifdef IMASK_EXTERNAL
	jal	hw_setintrmask
	nop
#endif
	REG_L	ra, PCB_CONTEXT+10*REGSZ(t3)
	REG_L	v0, PCB_CONTEXT+11*REGSZ(t3)
	REG_L	v1, PCB_CONTEXT+12*REGSZ(t3)
#ifndef IMASK_EXTERNAL
	ctc0	v1, COP_0_ICR		# XXX RM7000
#endif
	mtc0	v0, COP_0_STATUS_REG
	ITLBNOPFIX
	j	ra
	nop
4:
	PANIC("cpu_switch")			# nothing in queue
END(cpu_switch)
