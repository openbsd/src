/*	$OpenBSD: cpuswitch.S,v 1.3 2004/06/13 21:49:13 niklas Exp $	*/
/*	$NetBSD: cpuswitch.S,v 1.41 2003/11/15 08:44:18 scw Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpuswitch.S
 *
 * cpu switching functions
 *
 * Created      : 15/10/94
 */

#include "assym.h"
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/asm.h>

/* LINTSTUB: include <sys/param.h> */
	
#undef IRQdisable
#undef IRQenable

/*
 * New experimental definitions of IRQdisable and IRQenable
 * These keep FIQ's enabled since FIQ's are special.
 */

#define IRQdisable \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

#define IRQenable \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

/*
 * These are used for switching the translation table/DACR.
 * Since the vector page can be invalid for a short time, we must
 * disable both regular IRQs *and* FIQs.
 *
 * XXX: This is not necessary if the vector table is relocated.
 */
#define IRQdisableALL \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit | F32_bit) ; \
	msr	cpsr_c, r14

#define IRQenableALL \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit | F32_bit) ; \
	msr	cpsr_c, r14

	.text

.Lwhichqs:
	.word	_C_LABEL(whichqs)

.Lqs:
	.word	_C_LABEL(qs)

/*
 * On entry
 *	r0 = process
 */

ENTRY(setrunqueue)
	/*
	 * Local register usage
	 * 	r0 = process
	 * 	r1 = queue
	 * 	r2 = &qs[queue] and temp
	 * 	r3 = temp
	 *	r12 = whichqs
	 */
#ifdef DIAGNOSTIC
	ldr	r1, [r0, #(P_BACK)]
	teq	r1, #0x00000000
	bne	Lsetrunqueue_erg

	ldr	r1, [r0, #(P_WCHAN)]
	teq	r1, #0x00000000
	bne	Lsetrunqueue_erg
#endif

	/* Get the priority of the queue */
	ldrb	r1, [r0, #(P_PRIORITY)]
	mov	r1, r1, lsr #2

	/* Indicate that there is a process on this queue */
	ldr	r12, .Lwhichqs
	ldr	r2, [r12]
	mov	r3, #0x00000001
	mov	r3, r3, lsl r1
	orr	r2, r2, r3
	str	r2, [r12]

	/* Get the address of the queue */
	ldr	r2, .Lqs
	add	r1, r2, r1, lsl # 3

	/* Hook the process in */
	str	r1, [r0, #(P_FORW)]
	ldr	r2, [r1, #(P_BACK)]

	str	r0, [r1, #(P_BACK)]
#ifdef DIAGNOSTIC
	teq	r2, #0x00000000
	beq	Lsetrunqueue_erg
#endif
	str	r0, [r2, #(P_FORW)]
	str	r2, [r0, #(P_BACK)]

	mov	pc, lr

#ifdef DIAGNOSTIC
Lsetrunqueue_erg:
	mov	r2, r1
	mov	r1, r0
	add	r0, pc, #Ltext1 - . - 8
	bl	_C_LABEL(printf)

	ldr	r2, .Lqs
	ldr	r1, [r2]
	add	r0, pc, #Ltext2 - . - 8
	b	_C_LABEL(panic)	

Ltext1:
	.asciz	"setrunqueue : %08x %08x\n"
Ltext2:
	.asciz	"setrunqueue : [qs]=%08x qs=%08x\n"
	.align	0
#endif

/*
 * On entry
 *	r0 = process
 */

ENTRY(remrunqueue)
	/*
	 * Local register usage
	 *	r0 = oldproc
	 * 	r1 = queue
	 * 	r2 = &qs[queue] and scratch
	 *	r3 = scratch
	 *	r12 = whichqs
	 */

	/* Get the priority of the queue */
	ldrb	r1, [r0, #(P_PRIORITY)]
	mov	r1, r1, lsr #2

	/* Unhook the process */
	ldr	r2, [r0, #(P_FORW)]
	ldr	r3, [r0, #(P_BACK)]

	str	r3, [r2, #(P_BACK)]
	str	r2, [r3, #(P_FORW)]

	/* If the queue is now empty clear the queue not empty flag */
	teq	r2, r3

	/* This could be reworked to avoid the use of r4 */
	ldreq	r12, .Lwhichqs
	ldreq	r2, [r12]
	moveq	r3, #0x00000001
	moveq	r3, r3, lsl r1
	biceq	r2, r2, r3
	streq	r2, [r12]

	/* Remove the back pointer for the process */
	mov	r1, #0x00000000
	str	r1, [r0, #(P_BACK)]

	mov	pc, lr


/*
 * cpuswitch()
 *
 * preforms a process context switch.
 * This function has several entry points
 */

#ifdef MULTIPROCESSOR
.Lcpu_info_store:
	.word	_C_LABEL(cpu_info_store)
.Lcurproc:
	/* FIXME: This is bogus in the general case. */
	.word	_C_LABEL(cpu_info_store) + CI_CURLWP

.Lcurpcb:
	.word	_C_LABEL(cpu_info_store) + CI_CURPCB
#else
.Lcurproc:
	.word	_C_LABEL(curproc)

.Lcurpcb:
	.word	_C_LABEL(curpcb)
#endif

.Lwant_resched:
	.word	_C_LABEL(want_resched)

.Lcpufuncs:	
	.word	_C_LABEL(cpufuncs)
	
#ifndef MULTIPROCESSOR
	.data
	.global	_C_LABEL(curpcb)
_C_LABEL(curpcb):
	.word	0x00000000
	.text
#endif

.Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

.Lcpu_do_powersave:
	.word	_C_LABEL(cpu_do_powersave)

.Lpmap_kernel_cstate:
	.word	(kernel_pmap_store + PMAP_CSTATE)

.Llast_cache_state_ptr:
	.word	_C_LABEL(pmap_cache_state)

/*
 * Idle loop, exercised while waiting for a process to wake up.
 *
 * NOTE: When we jump back to .Lswitch_search, we must have a
 * pointer to whichqs in r7, which is what it is when we arrive
 * here.
 */
/* LINTSTUB: Ignore */
ASENTRY_NP(idle)
	ldr	r6, .Lcpu_do_powersave
	IRQenable			/* Enable interrupts */
	ldr	r6, [r6]		/* r6 = cpu_do_powersave */

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_unlock_idle)
#endif

	/* Drop to spl0 (returns the current spl level in r0). */
#ifdef __NEWINTR
	mov	r0, #(IPL_NONE)
	bl	_C_LABEL(_spllower)
#else /* ! __NEWINTR */
	mov	r0, #(_SPL_0)
	bl	_C_LABEL(splx)
#endif /* __NEWINTR */

	teq	r6, #0			/* cpu_do_powersave non zero? */
	ldrne	r6, .Lcpufuncs
	mov	r4, r0			/* Old interrupt level to r4 */
	ldrne	r6, [r6, #(CF_SLEEP)]

	/*
	 * Main idle loop.
	 * r6 points to power-save idle function if required, else NULL.
	 */
1:	ldr	r3, [r7]		/* r3 = sched_whichqs */
	teq	r3, #0
	bne	2f			/* We have work to do */
	teq	r6, #0			/* Powersave idle? */
	beq	1b			/* Nope. Just sit-n-spin. */

	/*
	 * Before going into powersave idle mode, disable interrupts
	 * and check sched_whichqs one more time.
	 */
	IRQdisableALL
	ldr	r3, [r7]
	mov	r0, #0
	teq	r3, #0			/* sched_whichqs still zero? */
	moveq	lr, pc
	moveq	pc, r6			/* If so, do powersave idle */
	IRQenableALL
	b	1b			/* Back around */

	/*
	 * sched_whichqs indicates that at least one proc is ready to run.
	 * Restore the original interrupt priority level, grab the
	 * scheduler lock if necessary, and jump back into cpu_switch.
	 */
2:	mov	r0, r4
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(splx)
	adr	lr, .Lswitch_search
	b	_C_LABEL(sched_lock_idle)
#else
	adr	lr, .Lswitch_search
	b	_C_LABEL(splx)
#endif


/*
 * Find a new lwp to run, save the current context and
 * load the new context
 *
 * Arguments:
 *	r0	'struct proc *' of the current LWP
 */

ENTRY(cpu_switch)
/*
 * Local register usage. Some of these registers are out of date.
 * r1 = oldproc
 * r2 = spl level
 * r3 = whichqs
 * r4 = queue
 * r5 = &qs[queue]
 * r6 = newlwp
 * r7 = scratch
 */
	stmfd	sp!, {r4-r7, lr}

	/*
	 * Indicate that there is no longer a valid process (curlwp = 0).
	 * Zero the current PCB pointer while we're at it.
	 */
	ldr	r7, .Lcurproc
	ldr	r6, .Lcurpcb
	mov	r2, #0x00000000
	str	r2, [r7]		/* curproc = NULL */
	str	r2, [r6]		/* curpcb = NULL */

	/* stash the old proc while we call functions */
	mov	r5, r0

	/* First phase : find a new proc */
	ldr	r7, .Lwhichqs

	/* rem: r5 = old proc */
	/* rem: r7 = &whichqs */

.Lswitch_search:
	IRQdisable

	/* Do we have any active queues  */
	ldr	r3, [r7]

	/* If not we must idle until we do. */
	teq	r3, #0x00000000
	beq	_ASM_LABEL(idle)

	/* put old proc back in r1 */
	mov	r1, r5

	/* rem: r1 = old proc */
	/* rem: r3 = whichqs */
	/* rem: interrupts are disabled */

	/* used further down, saves SA stall */
	ldr	r6, .Lqs

	/*
	 * We have found an active queue. Currently we do not know which queue
	 * is active just that one of them is.
	 */
	/* Non-Xscale version of the ffs algorithm devised by d.seal and
	 * posted to comp.sys.arm on 16 Feb 1994.
	 */
 	rsb	r5, r3, #0
 	ands	r0, r3, r5

#ifndef __XSCALE__
	adr	r5, .Lcpu_switch_ffs_table

				    /* X = R0 */
	orr	r4, r0, r0, lsl #4  /* r4 = X * 0x11 */ 
	orr	r4, r4, r4, lsl #6  /* r4 = X * 0x451 */
	rsb	r4, r4, r4, lsl #16 /* r4 = X * 0x0450fbaf */
	
	/* now lookup in table indexed on top 6 bits of a4 */
	ldrb	r4, [ r5, r4, lsr #26 ]

#else	/* __XSCALE__ */
	clz	r4, r0
	rsb	r4, r4, #31
#endif	/* __XSCALE__ */

	/* rem: r0 = bit mask of chosen queue (1 << r4) */
	/* rem: r1 = old proc */
	/* rem: r3 = whichqs */
	/* rem: r4 = queue number */
	/* rem: interrupts are disabled */

	/* Get the address of the queue (&qs[queue]) */
	add	r5, r6, r4, lsl #3

	/*
	 * Get the proc from the queue and place the next process in
	 * the queue at the head. This basically unlinks the lwp at
	 * the head of the queue.
	 */
	ldr	r6, [r5, #(P_FORW)]

#ifdef DIAGNOSTIC
	cmp	r6, r5
	beq	.Lswitch_bogons
#endif

	/* rem: r6 = new proc */
	ldr	r7, [r6, #(P_FORW)]
	str	r7, [r5, #(P_FORW)]	

	/*
	 * Test to see if the queue is now empty. If the head of the queue
	 * points to the queue itself then there are no more procs in
	 * the queue. We can therefore clear the queue not empty flag held
	 * in r3.
	 */

	teq	r5, r7
	biceq	r3, r3, r0

	/* rem: r0 = bit mask of chosen queue (1 << r4) - NOT NEEDED AN MORE */

	/* Fix the back pointer for the lwp now at the head of the queue. */
	ldr	r0, [r6, #(P_BACK)]
	str	r0, [r7, #(P_BACK)]

	/* Update the RAM copy of the queue not empty flags word. */
	ldreq	r7, .Lwhichqs
	streq	r3, [r7]

	/* rem: r1 = old proc */
	/* rem: r3 = whichqs - NOT NEEDED ANY MORE */
	/* rem: r4 = queue number - NOT NEEDED ANY MORE */
	/* rem: r6 = new proc */
	/* rem: interrupts are disabled */

	/* Clear the want_resched flag */
	ldr	r7, .Lwant_resched
	mov	r0, #0x00000000
	str	r0, [r7]

	/*
	 * Clear the back pointer of the proc we have removed from
	 * the head of the queue. The new proc is isolated now.
	 */
	str	r0, [r6, #(P_BACK)]

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	/*
	 * unlock the sched_lock, but leave interrupts off, for now.
	 */
	mov	r7, r1
	bl	_C_LABEL(sched_unlock_idle)
	mov	r1, r7
#endif


.Lswitch_resume:
	/* rem: r1 = old proc */
	/* rem: r4 = return value [not used if came from cpu_switchto()] */
	/* rem: r6 = new process */
	/* rem: interrupts are disabled */

#ifdef MULTIPROCESSOR
	/* XXX use curcpu() */
	ldr	r0, .Lcpu_info_store
	str	r0, [r6, #(P_CPU)]
#else
	/* l->l_cpu initialized in fork1() for single-processor */
#endif

	/* Process is now on a processor. */
	mov	r0, #SONPROC			/* p->p_stat = SONPROC */
	strb	r0, [r6, #(P_STAT)]

	/* We have a new curproc now so make a note it */
	ldr	r7, .Lcurproc
	str	r6, [r7]

	/* Hook in a new pcb */
	ldr	r7, .Lcurpcb
	ldr	r0, [r6, #(P_ADDR)]
	str	r0, [r7]

	/* At this point we can allow IRQ's again. */
	IRQenable

	/* rem: r1 = old proc */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: interrupts are enabled */

	/*
	 * If the new process is the same as the process that called
	 * cpu_switch() then we do not need to save and restore any
	 * contexts. This means we can make a quick exit.
	 * The test is simple if curproc on entry (now in r1) is the
	 * same as the proc removed from the queue we can jump to the exit.
	 */
	teq	r1, r6
	moveq	r4, #0x00000000		/* default to "didn't switch" */
	beq	.Lswitch_return

	/*
	 * At this point, we are guaranteed to be switching to
	 * a new proc.
	 */
	mov	r4, #0x00000001

	/* Remember the old proc in r0 */
	mov	r0, r1

	/*
	 * If the old proc on entry to cpu_switch was zero then the
	 * process that called it was exiting. This means that we do
	 * not need to save the current context. Instead we can jump
	 * straight to restoring the context for the new process.
	 */
	teq	r0, #0x00000000
	beq	.Lswitch_exited

	/* rem: r0 = old proc */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: interrupts are enabled */

	/* Stage two : Save old context */

	/* Get the user structure for the old proc. */
	ldr	r1, [r0, #(P_ADDR)]

	/* Save all the registers in the old proc's pcb */
#ifndef __XSCALE__
	add	r7, r1, #(PCB_R8)
	stmia	r7, {r8-r13}
#else
	strd	r8, [r1, #(PCB_R8)]
	strd	r10, [r1, #(PCB_R10)]
	strd	r12, [r1, #(PCB_R12)]
#endif

	/*
	 * NOTE: We can now use r8-r13 until it is time to restore
	 * them for the new process.
	 */

	/* Remember the old PCB. */
	mov	r8, r1

	/* r1 now free! */

	/* Get the user structure for the new process in r9 */
	ldr	r9, [r6, #(P_ADDR)]

	/*
	 * This can be optimised... We know we want to go from SVC32
	 * mode to UND32 mode
	 */
        mrs	r3, cpsr
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE | I32_bit)
        msr	cpsr_c, r2

	str	sp, [r8, #(PCB_UND_SP)]

        msr	cpsr_c, r3		/* Restore the old mode */

	/* rem: r0 = old proc */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: r8 = old PCB */
	/* rem: r9 = new PCB */
	/* rem: interrupts are enabled */

	/* What else needs to be saved  Only FPA stuff when that is supported */

	/* Third phase : restore saved context */

	/* rem: r0 = old proc */
	/* rem: r4 = return value */
	/* rem: r6 = new proc */
	/* rem: r8 = old PCB */
	/* rem: r9 = new PCB */
	/* rem: interrupts are enabled */

	/*
	 * Get the new L1 table pointer into r11.  If we're switching to
	 * an LWP with the same address space as the outgoing one, we can
	 * skip the cache purge and the TTB load.
	 *
	 * To avoid data dep stalls that would happen anyway, we try
	 * and get some useful work done in the mean time.
	 */
	ldr	r10, [r8, #(PCB_PAGEDIR)]	/* r10 = old L1 */
	ldr	r11, [r9, #(PCB_PAGEDIR)]	/* r11 = new L1 */

	ldr	r0, [r8, #(PCB_DACR)]		/* r0 = old DACR */
	ldr	r1, [r9, #(PCB_DACR)]		/* r1 = new DACR */
	ldr	r8, [r9, #(PCB_CSTATE)]		/* r8 = &new_pmap->pm_cstate */
	ldr	r5, .Llast_cache_state_ptr	/* Previous thread's cstate */

	teq	r10, r11			/* Same L1? */
	ldr	r5, [r5]
	cmpeq	r0, r1				/* Same DACR? */
	beq	.Lcs_context_switched		/* yes! */

	ldr	r3, .Lblock_userspace_access
	mov	r12, #0
	cmp	r5, #0				/* No last vm? (switch_exit) */
	beq	.Lcs_cache_purge_skipped	/* No, we can skip cache flsh */

	mov	r2, #DOMAIN_CLIENT
	cmp	r1, r2, lsl #(PMAP_DOMAIN_KERNEL * 2) /* Sw to kernel thread? */
	beq	.Lcs_cache_purge_skipped	/* Yup. Don't flush cache */

	cmp	r5, r8				/* Same userland VM space? */
	ldrneb	r12, [r5, #(CS_CACHE_ID)]	/* Last VM space cache state */

	/*
	 * We're definately switching to a new userland VM space,
	 * and the previous userland VM space has yet to be flushed
	 * from the cache/tlb.
	 *
	 * r12 holds the previous VM space's cs_cache_id state
	 */
	tst	r12, #0xff			/* Test cs_cache_id */
	beq	.Lcs_cache_purge_skipped	/* VM space is not in cache */

	/*
	 * Definately need to flush the cache.
	 * Mark the old VM space as NOT being resident in the cache.
	 */
	mov	r2, #0x00000000
	strb	r2, [r5, #(CS_CACHE_ID)]
	strb	r2, [r5, #(CS_CACHE_D)]

	/*
	 * Don't allow user space access between the purge and the switch.
	 */
	mov	r2, #0x00000001
	str	r2, [r3]

	stmfd	sp!, {r0-r3}
	ldr	r1, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r1, #CF_IDCACHE_WBINV_ALL]
	ldmfd	sp!, {r0-r3}

.Lcs_cache_purge_skipped:
	/* rem: r1 = new DACR */
	/* rem: r3 = &block_userspace_access */
	/* rem: r4 = return value */
	/* rem: r5 = &old_pmap->pm_cstate (or NULL) */
	/* rem: r6 = new proc */
	/* rem: r8 = &new_pmap->pm_cstate */
	/* rem: r9 = new PCB */
	/* rem: r10 = old L1 */
	/* rem: r11 = new L1 */

	mov	r2, #0x00000000
	ldr	r7, [r9, #(PCB_PL1VEC)]

	/*
	 * At this point we need to kill IRQ's again.
	 *
	 * XXXSCW: Don't need to block FIQs if vectors have been relocated
	 */
	IRQdisableALL

	/*
	 * Interrupts are disabled so we can allow user space accesses again
	 * as none will occur until interrupts are re-enabled after the
	 * switch.
	 */
	str	r2, [r3]

	/*
	 * Ensure the vector table is accessible by fixing up the L1
	 */
	cmp	r7, #0			/* No need to fixup vector table? */
	ldrne	r2, [r7]		/* But if yes, fetch current value */
	ldrne	r0, [r9, #(PCB_L1VEC)]	/* Fetch new vector_page value */
	mcr	p15, 0, r1, c3, c0, 0	/* Update DACR for new context */
	cmpne	r2, r0			/* Stuffing the same value? */
#ifndef PMAP_INCLUDE_PTE_SYNC
	strne	r0, [r7]		/* Nope, update it */
#else
	beq	.Lcs_same_vector
	str	r0, [r7]		/* Otherwise, update it */

	/*
	 * Need to sync the cache to make sure that last store is
	 * visible to the MMU.
	 */
	ldr	r2, .Lcpufuncs
	mov	r0, r7
	mov	r1, #4
	mov	lr, pc
	ldr	pc, [r2, #CF_DCACHE_WB_RANGE]

.Lcs_same_vector:
#endif /* PMAP_INCLUDE_PTE_SYNC */

	cmp	r10, r11		/* Switching to the same L1? */
	ldr	r10, .Lcpufuncs
	beq	.Lcs_same_l1		/* Yup. */

	/*
	 * Do a full context switch, including full TLB flush.
	 */
	mov	r0, r11
	mov	lr, pc
	ldr	pc, [r10, #CF_CONTEXT_SWITCH]

	/*
	 * Mark the old VM space as NOT being resident in the TLB
	 */
	mov	r2, #0x00000000
	cmp	r5, #0
	strneh	r2, [r5, #(CS_TLB_ID)]
	b	.Lcs_context_switched

	/*
	 * We're switching to a different process in the same L1.
	 * In this situation, we only need to flush the TLB for the
	 * vector_page mapping, and even then only if r7 is non-NULL.
	 */
.Lcs_same_l1:
	cmp	r7, #0
	movne	r0, #0			/* We *know* vector_page's VA is 0x0 */
	movne	lr, pc
	ldrne	pc, [r10, #CF_TLB_FLUSHID_SE]

.Lcs_context_switched:
	/* rem: r8 = &new_pmap->pm_cstate */

	/* XXXSCW: Safe to re-enable FIQs here */

	/*
	 * The new VM space is live in the cache and TLB.
	 * Update its cache/tlb state, and if it's not the kernel
	 * pmap, update the 'last cache state' pointer.
	 */
	mov	r2, #-1
	ldr	r5, .Lpmap_kernel_cstate
	ldr	r0, .Llast_cache_state_ptr
	str	r2, [r8, #(CS_ALL)]
	cmp	r5, r8
	strne	r8, [r0]

	/* rem: r4 = return value */
	/* rem: r6 = new proc */
	/* rem: r9 = new PCB */

	/*
	 * This can be optimised... We know we want to go from SVC32
	 * mode to UND32 mode
	 */
        mrs	r3, cpsr
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE)
        msr	cpsr_c, r2

	ldr	sp, [r9, #(PCB_UND_SP)]

        msr	cpsr_c, r3		/* Restore the old mode */

	/* Restore all the save registers */
#ifndef __XSCALE__
	add	r7, r9, #PCB_R8
	ldmia	r7, {r8-r13}

	sub	r7, r7, #PCB_R8		/* restore PCB pointer */
#else
	mov	r7, r9
	ldr	r8, [r7, #(PCB_R8)]
	ldr	r9, [r7, #(PCB_R9)]
	ldr	r10, [r7, #(PCB_R10)]
	ldr	r11, [r7, #(PCB_R11)]
	ldr	r12, [r7, #(PCB_R12)]
	ldr	r13, [r7, #(PCB_SP)]
#endif

#if 0
	ldr	r5, [r6, #(L_PROC)]	/* fetch the proc for below */
#else
	mov	r5, r6
#endif

	/* rem: r4 = return value */
	/* rem: r5 = new proc's proc */
	/* rem: r6 = new proc */
	/* rem: r7 = new pcb */

#ifdef ARMFPE
	add	r0, r7, #(USER_SIZE) & 0x00ff
	add	r0, r0, #(USER_SIZE) & 0xff00 
	bl	_C_LABEL(arm_fpe_core_changecontext)
#endif

	/* We can enable interrupts again */
	IRQenableALL

	/* rem: r4 = return value */
	/* rem: r5 = new proc's proc */
	/* rem: r6 = new proc */
	/* rem: r7 = new PCB */

#if 0
	/* 
	 * Check for restartable atomic sequences (RAS).
	 */

	ldr	r2, [r5, #(P_RASLIST)]
	ldr	r1, [r7, #(PCB_TF)]	/* r1 = trapframe (used below) */
	teq	r2, #0			/* p->p_nras == 0? */
	bne	.Lswitch_do_ras		/* no, check for one */
#endif

.Lswitch_return:
	/* cpu_switch returns 1 == switched, 0 == didn't switch */
	mov	r0, r4

	/*
	 * Pull the registers that got pushed when either savectx() or
	 * cpu_switch() was called and return.
	 */
	ldmfd	sp!, {r4-r7, pc}

#if 0
.Lswitch_do_ras:
	ldr	r1, [r1, #(TF_PC)]	/* second ras_lookup() arg */
	mov	r0, r5			/* first ras_lookup() arg */
	bl	_C_LABEL(ras_lookup)
	cmn	r0, #1			/* -1 means "not in a RAS" */
	ldrne	r1, [r7, #(PCB_TF)]
	strne	r0, [r1, #(TF_PC)]
	b	.Lswitch_return
#endif

.Lswitch_exited:
	/*
	 * We skip the cache purge because switch_exit() already did it.
	 * Load up registers the way .Lcs_cache_purge_skipped expects.
	 * Userpsace access already blocked by switch_exit().
	 */
	ldr	r9, [r6, #(P_ADDR)]		/* r9 = new PCB */
	ldr	r3, .Lblock_userspace_access
	mrc	p15, 0, r10, c2, c0, 0		/* r10 = old L1 */
	mov	r5, #0				/* No previous cache state */
	ldr	r1, [r9, #(PCB_DACR)]		/* r1 = new DACR */
	ldr	r8, [r9, #(PCB_CSTATE)]		/* r8 = new cache state */
	ldr	r11, [r9, #(PCB_PAGEDIR)]	/* r11 = new L1 */
	b	.Lcs_cache_purge_skipped


#ifdef DIAGNOSTIC
.Lswitch_bogons:
	adr	r0, .Lswitch_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b	1b

.Lswitch_panic_str:
	.asciz	"cpu_switch: sched_qs empty with non-zero sched_whichqs!\n"
#endif

/*
 * cpu_switchto(struct proc *current, struct proc *next)
 * Switch to the specified next LWP
 * Arguments:
 *
 *	r0	'struct proc *' of the current LWP
 *	r1	'struct proc *' of the LWP to switch to
 */
ENTRY(cpu_switchto)
	stmfd	sp!, {r4-r7, lr}

	mov	r6, r1		/* save new proc */

#if defined(LOCKDEBUG)
	mov	r5, r0		/* save old proc */
	bl	_C_LABEL(sched_unlock_idle)
	mov	r1, r5
#else
	mov	r1, r0
#endif

	IRQdisable

	/*
	 * Okay, set up registers the way cpu_switch() wants them,
	 * and jump into the middle of it (where we bring up the
	 * new process).
	 *
	 * r1 = old proc (r6 = new proc)
	 */
	b	.Lswitch_resume

/*
 * void switch_exit(struct proc *l, struct proc *l0,
 *    void (*exit)(struct proc *));
 * Switch to proc0's saved context and deallocate the address space and kernel
 * stack for l.  Then jump into cpu_switch(), as if we were in proc0 all along.
 */

/* LINTSTUB: Func: void switch_exit(struct proc *l, struct proc *l0,
    void (*func)(struct proc *)) */
ENTRY(switch_exit)
	/*
	 * The process is going away, so we can use callee-saved
	 * registers here without having to save them.
	 */

	mov	r4, r0
	ldr	r0, .Lcurproc

	mov	r5, r1
	ldr	r1, .Lblock_userspace_access

	mov	r6, r2

	/*
	 * r4 = proc
	 * r5 = proc0
	 * r6 = exit func
	 */

	mov	r2, #0x00000000		/* curproc = NULL */
	str	r2, [r0]

	/*
	 * We're about to clear both the cache and the TLB.
	 * Make sure to zap the 'last cache state' pointer since the
	 * pmap might be about to go away. Also ensure the outgoing
	 * VM space's cache state is marked as NOT resident in the
	 * cache, and that proc0's cache state IS resident.
	 */
	ldr	r7, [r4, #(P_ADDR)]		/* r7 = old proc's PCB */
	ldr	r0, .Llast_cache_state_ptr	/* Last userland cache state */
	ldr	r9, [r7, #(PCB_CSTATE)]		/* Fetch cache state pointer */
	ldr	r3, [r5, #(P_ADDR)]		/* r3 = proc0's PCB */
	str	r2, [r0]			/* No previous cache state */
	str	r2, [r9, #(CS_ALL)]		/* Zap old proc's cache state */
	ldr	r3, [r3, #(PCB_CSTATE)]		/* proc0's cache state */
	mov	r2, #-1
	str	r2, [r3, #(CS_ALL)]		/* proc0 is in da cache! */

	/*
	 * Don't allow user space access between the purge and the switch.
	 */
	mov	r2, #0x00000001
	str	r2, [r1]

	/* Switch to proc0 context */

	ldr	r9, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r9, #CF_IDCACHE_WBINV_ALL]

	ldr	r0, [r7, #(PCB_PL1VEC)]
	ldr	r1, [r7, #(PCB_DACR)]

	/*
	 * r0 = Pointer to L1 slot for vector_page (or NULL)
	 * r1 = proc0's DACR
	 * r4 = proc we're switching from
	 * r5 = proc0
	 * r6 = exit func
	 * r7 = proc0's PCB
	 * r9 = cpufuncs
	 */

	IRQdisableALL

	/*
	 * Ensure the vector table is accessible by fixing up proc0's L1
	 */
	cmp	r0, #0			/* No need to fixup vector table? */
	ldrne	r3, [r0]		/* But if yes, fetch current value */
	ldrne	r2, [r7, #(PCB_L1VEC)]	/* Fetch new vector_page value */
	mcr	p15, 0, r1, c3, c0, 0	/* Update DACR for proc0's context */
	cmpne	r3, r2			/* Stuffing the same value? */
	strne	r2, [r0]		/* Store if not. */

#ifdef PMAP_INCLUDE_PTE_SYNC
	/*
	 * Need to sync the cache to make sure that last store is
	 * visible to the MMU.
	 */
	movne	r1, #4
	movne	lr, pc
	ldrne	pc, [r9, #CF_DCACHE_WB_RANGE]
#endif /* PMAP_INCLUDE_PTE_SYNC */

	/*
	 * Note: We don't do the same optimisation as cpu_switch() with
	 * respect to avoiding flushing the TLB if we're switching to
	 * the same L1 since this process' VM space may be about to go
	 * away, so we don't want *any* turds left in the TLB.
	 */

	/* Switch the memory to the new process */
	ldr	r0, [r7, #(PCB_PAGEDIR)]
	mov	lr, pc
	ldr	pc, [r9, #CF_CONTEXT_SWITCH]

	ldr	r0, .Lcurpcb
   
	/* Restore all the save registers */
#ifndef __XSCALE__
	add	r1, r7, #PCB_R8
	ldmia	r1, {r8-r13}
#else
	ldr	r8, [r7, #(PCB_R8)]
	ldr	r9, [r7, #(PCB_R9)]
	ldr	r10, [r7, #(PCB_R10)]
	ldr	r11, [r7, #(PCB_R11)]
	ldr	r12, [r7, #(PCB_R12)]
	ldr	r13, [r7, #(PCB_SP)]
#endif
	str	r7, [r0]	/* curpcb = proc0's PCB */

	IRQenableALL

	/*
	 * Schedule the vmspace and stack to be freed.
	 */
	mov	r0, r4			/* {proc_}exit2(l) */
	mov	lr, pc
	mov	pc, r6

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_lock_idle)
#endif

	ldr	r7, .Lwhichqs		/* r7 = &whichqs */
	mov	r5, #0x00000000		/* r5 = old proc = NULL */
	b	.Lswitch_search

/* LINTSTUB: Func: void savectx(struct pcb *pcb) */
ENTRY(savectx)
	/*
	 * r0 = pcb
	 */

	/* Push registers.*/
	stmfd	sp!, {r4-r7, lr}

	/* Store all the registers in the process's pcb */
#ifndef __XSCALE__
	add	r2, r0, #(PCB_R8)
	stmia	r2, {r8-r13}
#else
	strd	r8, [r0, #(PCB_R8)]
	strd	r10, [r0, #(PCB_R10)]
	strd	r12, [r0, #(PCB_R12)]
#endif

	/* Pull the regs of the stack */
	ldmfd	sp!, {r4-r7, pc}

ENTRY(proc_trampoline)
#ifdef __NEWINTR
	mov	r0, #(IPL_NONE)
	bl	_C_LABEL(_spllower)
#else /* ! __NEWINTR */
	mov	r0, #(_SPL_0)
	bl	_C_LABEL(splx)
#endif /* __NEWINTR */

#ifdef MULTIPROCESSOR
	bl	_C_LABEL(proc_trampoline_mp)
#endif
	mov	r0, r5
	mov	r1, sp
	mov	lr, pc
	mov	pc, r4

	/* Kill irq's */
        mrs     r0, cpsr
        orr     r0, r0, #(I32_bit)
        msr     cpsr_c, r0

	PULLFRAME

	movs	pc, lr			/* Exit */

#ifndef __XSCALE__
	.type .Lcpu_switch_ffs_table, _ASM_TYPE_OBJECT;
.Lcpu_switch_ffs_table:
/* same as ffs table but all nums are -1 from that */
/*               0   1   2   3   4   5   6   7           */
	.byte	 0,  0,  1, 12,  2,  6,  0, 13  /*  0- 7 */
	.byte	 3,  0,  7,  0,  0,  0,  0, 14  /*  8-15 */
	.byte	10,  4,  0,  0,  8,  0,  0, 25  /* 16-23 */
	.byte	 0,  0,  0,  0,  0, 21, 27, 15  /* 24-31 */
	.byte	31, 11,  5,  0,  0,  0,  0,  0	/* 32-39 */
	.byte	 9,  0,  0, 24,  0,  0, 20, 26  /* 40-47 */
	.byte	30,  0,  0,  0,  0, 23,  0, 19  /* 48-55 */
	.byte   29,  0, 22, 18, 28, 17, 16,  0  /* 56-63 */
#endif	/* !__XSCALE_ */
