/* $NetBSD: cpuswitch.S,v 1.6 1996/03/27 21:24:39 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
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
 * cpu.S
 *
 * cpu switching functions
 *
 * Created      : 15/10/94
 */

#include "assym.h"
#include <machine/param.h>
#include <machine/cpu.h>

sp	.req	r13
lr	.req	r14
pc	.req	r15

/*
 * PULLFRAME - macro to pull a trap frame from the stack in the current mode
 * Since the current mode is used, the SVC R14 field is ignored.
 */

#define PULLFRAME \
        ldr     r0, [sp], #0x0004;      /* Get the SPSR from stack */ \
        msr     spsr_all, r0; \
	add	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
        ldmdb   sp, {r0-r14}^;		/* Restore the registers (user mode) */ \
        mov     r0, r0;                 /* NOP for previous instruction */ \
 	add	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
 	ldr	lr, [sp], #0x0004;	/* Pull the return address */

/*
 * setrunqueue() and remrunqueue()
 *
 * Functions to add and remove a process for the run queue.
 */

	.text

Lwhichqs:
	.word	_whichqs

Lqs:
	.word	_qs

/*
 * On entry
 *	r0 = process
 */

	.global	_setrunqueue
_setrunqueue:
/*
 * Local register usage
 * 	r0 = process
 * 	r1 = queue
 * 	r2 = &qs[queue]
 * 	r3 = temp
 *	r4 = whichqs
 */
	stmfd	sp!, {r4}

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

	ldr	r4, Lwhichqs
	ldr	r2, [r4]
	mov	r3, #0x00000001
	mov	r3, r3, lsl r1
	orr	r2, r2, r3
	str	r2, [r4]

/* Get the address of the queue */

	ldr	r2, Lqs
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

	ldmfd	sp!, {r4}

	mov	pc, lr

#ifdef DIAGNOSTIC
Lsetrunqueue_erg:
	mov	r2, r1
	mov	r1, r0
	add	r0, pc, #Ltext1 - . - 8
	bl	_printf

	ldr	r2, Lqs
	ldr	r1, [r2]
	add	r0, pc, #Ltext2 - . - 8
	b	_panic	

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

	.global	_remrunqueue
_remrunqueue:

/*
 * Local register usage
 *	r0 = oldproc
 * 	r1 = queue
 * 	r2 = &qs[queue]
 *	r3 = scratch
 *	r4 = whichqs
 */
	stmfd	sp!, {r4}

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

	ldreq	r4, Lwhichqs
	ldreq	r2, [r4]
	moveq	r3, #0x00000001
	moveq	r3, r3, lsl r1
	biceq	r2, r2, r3
	streq	r2, [r4]

/* Remove the back pointer for the process */

	mov	r1, #0x00000000
	str	r1, [r0, #(P_BACK)]

	ldmfd	sp!, {r4}

	mov	pc, lr


/*
 * cpuswitch()
 *
 * preforms a process context switch.
 * This function has several entry points
 */


Lcurproc:
	.word	_curproc

	.global	_curpcb

_curpcb:
	.word	0x00000000

Lcurpcb:
	.word	_curpcb

Lwant_resched:
	.word	_want_resched


/*
 * Idle loop, exercised while waiting for a process to wake up.
 */

	.global	_idle
_idle:
idle:

/* Enable interrupts */

	IRQenable

#ifdef CPU_ARM7500
/* ARM7500 has a suspend mode so use it ! [danny, does this work ? - mark] */

	mov	r7, #(IOMD_BASE)
	orr	r7, r7, #(IOMD_SUSPEND - IOMD_BASE)
	mov	r3, #0x00000001
	strb	r3, [r7]
#endif

/* Disable interrupts while we check for an active queue */

	IRQdisable
	ldr	r7, Lwhichqs
	ldr	r3, [r7]
	teq	r3, #0x00000000
	bne	sw1

/* All processes are still asleep so idle a while longer */

	b	idle


/*
 * Find a new process to run, save the current context and
 * load the new context
 */

	.global	_cpu_switch
_cpu_switch:
/*
 * Local register usage. Some of these registers are out of date.
 * r1 = oldproc
 * r2 = spl level
 * r3 = whichqs
 * r4 = queue
 * r5 = &qs[queue]
 * r6 = newproc
 * r7 = scratch
 */
	stmfd	sp!, {r4-r7, lr}

/*
 * Get the current process and indicate that there is no longer a valid
 * process (curproc = 0)
 */

	ldr	r7, Lcurproc
	ldr	r1, [r7]
	mov	r0, #0x00000000
	str	r0, [r7]

/* Zero the pcb */

	ldr	r7, Lcurpcb
	str	r0, [r7]

/* Lower the spl level to spl0 and get the current spl level. */

	mov	r7, r1

#ifdef spl0
	mov	r0, #(SPL_0)
	bl	_splx
#else
	bl	_spl0
#endif

/* Push the old spl level onto the stack */

	str	r0, [sp, #-0x0004]!

	mov	r1, r7

/* First phase : find a new process */

/* rem: r1 = old proc */

switch_search:
	IRQdisable

/* Do we have any active queues  */

	ldr	r7, Lwhichqs
	ldr	r3, [r7]

/* If not we must idle until we do. */

	teq	r3, #0x00000000
	beq	idle

sw1:
/* rem: r1 = old proc */
/* rem: r3 = whichqs */
/* rem: interrupts are disabled */

/*
 * Paranoid debug time ....
 * Is this overkill ? If we are not in SVC mode then things are
 * very sick and will probably have already died.
 */

#ifdef DIAGNOSTIC
	mrs	r4, cpsr_all
	and	r4, r4, #(PSR_MODE)
	teq	r4, #(PSR_SVC32_MODE)
	beq	switchmodeok

	add	r0, pc, #switchpanic - . - 8
	mrs	r1, cpsr_all
	bl	_panic

switchpanic:
	.asciz	"Yikes! In cpu_switch() but not in SVC mode (%08x)\n"
	.align	0

switchmodeok:
#endif

/*
 * We have found an active queue. Currently we do not know which queue
 * is active just that one of them is.
 * We must check each queue to find the active one.
 * r3 contains a bit for each of the 32 queues. A one indicates that
 * that the queue has something in it.
 */

/* This ffs() type routine code be optimised */

	mov	r4, #0x00000000

findqueue:
	mov	r0, #0x00000001
	mov	r0, r0, lsl r4

	tst	r3, r0
	addeq	r4, r4, #0x00000001
	beq	findqueue

/*
 * Ok we have found the active queue. The above code can never fail as
 * we only get to it if r3 != 0
 * r4 contains the number of the first queue found with a process in it.
 */

/* rem: r0 = bit mask of chosen queue (1 << r4) */
/* rem: r1 = old proc */
/* rem: r3 = whichqs */
/* rem: r4 = queue number */
/* rem: interrupts are disabled */

/* Get the address of the queue (&qs[queue]) */

	ldr	r5, Lqs
	add	r5, r5, r4, lsl #3

/*
 * Get the process from the queue and place the next process in the queue
 * at the head. This basically unlinks the process at the head of the queue.
 */
	ldr	r6, [r5, #(P_FORW)]

/* rem: r6 = new process */

	ldr	r7, [r6, #(P_FORW)]
	str	r7, [r5, #(P_FORW)]	

/*
 * Test to see if the queue is now empty. If the head of the queue points
 * to the queue itself then there are no more processes in the queue.
 * We can therefore clear the queue not empty flag held in r3.
 */

	teq	r5, r7
	biceq	r3, r3, r0

/* rem: r0 = bit mask of chosen queue (1 << r4) - NOT NEEDED AN MORE */

/* Fix the back pointer for the process now at the head of the queue. */

	ldr	r0, [r6, #(P_BACK)]
	str	r0, [r7, #(P_BACK)]

/* Update the RAM copy of the queue not empty flags word. */

	ldr	r7, Lwhichqs
	str	r3, [r7]

/* rem: r1 = old proc */
/* rem: r3 = whichqs - NOT NEEDED ANY MORE */
/* rem: r4 = queue number - NOT NEEDED ANY MORE */
/* rem: r6 = new process */
/* rem: interrupts are disabled */

/* Clear the want_resched flag */

	mov	r0, #0x00000000
	ldr	r7, Lwant_resched
	str	r0, [r7]

/*
 * Clear the back pointer of the process we have removed from the head
 * of the queue. The new process is isolated now.
 */

	mov	r0, #0x00000000
	str	r0, [r6, #(P_BACK)]

/* We have a new curproc now so make a note it */

	ldr	r7, Lcurproc
	str	r6, [r7]

/* Hook in a new pcb */

	ldr	r7, Lcurpcb
	ldr	r0, [r6, #(P_ADDR)]
	str	r0, [r7]

/* At this point we can allow IRQ's again. */

/*
	IRQenable
	IRQdisable
*/

/* rem: r1 = old proc */
/* rem: r6 = new process */
/* rem: interrupts are disabled */

/*
 * If the new process is the same as the process that called cpu_switch
 * Then we do not need to save and restore any contexts. This means
 * we can make a quick exit.
 * The test is simple if curproc on entry (now in r1) is the same as the
 * proc removed from the queue we can jump to the exit.
 */

	teq	r1, r6
	beq	switch_return

/*
 * If the curproc on entry to cpu_switch was zero then the process that
 * called it was exiting. This means that we do not need to save the current
 * context. Instead we can jump straight to restoring the context for
 * the new process.
 */

	teq	r1, #0x00000000
	beq	switch_exited

/* rem: r1 = old proc */
/* rem: r6 = new process */
/* rem: interrupts are disabled */

/* Stage two : Save old context */

/* Remember the old process in r0 */

	mov	r0, r1

/* Get the user structure for the old process. */

	ldr	r1, [r1, #(P_ADDR)]

/* Save all the registers in the old process's pcb */

	add	r7, r1, #(PCB_R8)
	stmia	r7, {r8-r13}

/*
 * This can be optimised... We know we want to go from SVC32 mode to UND32
 * mode
 */

        mrs	r3, cpsr_all
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE | I32_bit | F32_bit)
        msr	cpsr_all, r2

	str	sp, [r1, #(PCB_UND_SP)]

        msr	cpsr_all, r3		/* Restore the old mode */

/* rem: r0 = old proc */
/* rem: r0 = old pcb */
/* rem: r6 = new process */
/* rem: interrupts are disabled */

/* What else needs to be saved  Only FPA stuff when that is supported */

/* Third phase : restore saved context */

switch_exited:

/* At this point we need to kill IRQ's again. */

	mrs	r0, cpsr_all
	orr	r0, r0, #(I32_bit | F32_bit)
	msr	cpsr_all , r0

/*	IRQdisable*/

/* Get the user structure for the new process in r1 */

	ldr	r1, [r6, #(P_ADDR)]

/* Get the pagedir physical address for the process. */

	ldr	r0, [r1, #(PCB_PAGEDIR)]

/* Switch the memory to the new process */

/* For good measure we will flush the IDC as well */
        mcr     15, 0, r0, c7, c0, 0

/* Write the TTB */
        mcr     15, 0, r0, c2, c0, 0

/* If we have updated the TTB we must flush the TLB */
        mcr     15, 0, r0, c5, c0, 0

/* For good measure we will flush the IDC as well */
        mcr     15, 0, r0, c7, c0, 0

/* Make sure that pipeline is emptied */
        mov     r0, r0
        mov     r0, r0

/*
 * This can be optimised... We know we want to go from SVC32 mode to UND32
 * mode
 */

        mrs	r3, cpsr_all
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE)
        msr	cpsr_all, r2

	ldr	sp, [r1, #(PCB_UND_SP)]

        msr	cpsr_all, r3		/* Restore the old mode */

/* Restore all the save registers */

	add	r7, r1, #PCB_R8
	ldmia	r7, {r8-r13}

/* Remember the pcb currently in use */

	ldr	r7, Lcurpcb
	str	r1, [r7]

/* We can enable interrupts again */

/*
	IRQenable
*/

#ifdef ARMFPE
	add	r0, r1, #(USER_SIZE) & 0x00ff
	add	r0, r0, #(USER_SIZE) & 0xff00 
	bl	_arm_fpe_core_changecontext
#endif

switch_return:

/* We have a new curproc now so make a note it */

/*
	ldr	r7, Lcurproc
	str	r6, [r7]
*/

/* Get the spl level from the stack and update the current spl level */

	ldr	r0, [sp], #0x0004
	bl	_splx

/*	IRQenable*/

/* cpu_switch returns the proc it switched to. */

	mov	r0, r6

/*
 * Pull the registers that got pushed when either savectx or cpu_switch
 * was called and return.
 */
	ldmfd	sp!, {r4-r7, pc}

Lproc0:
	.word	_proc0

Lkernel_map:
	.word	_kernel_map


	.global	_switch_exit

_switch_exit:

/*
 * r0 = proc
 * r1 = proc0
 */

 	ldr	r1, Lproc0

/* In case we fault */

	mov	r2, #0x00000000
	ldr	r3, Lcurproc
	str	r2, [r3]

/*	ldr	r3, Lcurpcb
	str	r2, [r3]*/

/* Switch to proc0 context */

	IRQdisable

	ldr	r2, [r1, #(P_ADDR)]
	ldr	r3, [r2, #(PCB_PAGEDIR)]

/* For good measure we will flush the IDC as well */
        mcr     15, 0, r0, c7, c0, 0

/* Write the TTB */
        mcr     15, 0, r3, c2, c0, 0

/* If we have updated the TTB we must flush the TLB */
        mcr     15, 0, r0, c5, c0, 0

/* For good measure we will flush the IDC as well */
        mcr     15, 0, r0, c7, c0, 0

/* Make sure that pipeline is emptied */
        mov     r0, r0
        mov     r0, r0

/* Restore all the save registers */

	add	r7, r2, #PCB_R8
	ldmia	r7, {r8-r13}

/* This is not really needed ! */
/* Yes it is for the su and fu routines */

/*	ldr	sp, [r2, #(PCB_SP)]*/
	ldr	r3, Lcurpcb
	str	r2, [r3]

/*	IRQenable*/

	str	r0, [sp, #-0x0004]!

/* Thoroughly nuke the old process's resources. */

/* This has to be done here, before we lose the pmap */

	mov	r1, #0x00000000
	add	r2, r1, #NBPG
	ldr	r0, [r0, #(P_VMSPACE)]
	add	r0, r0, #(VM_PMAP)
	bl	_pmap_remove
	ldr	r0, [sp]

/*
 * Have to wait until we have switched to proc0 as the pmap gets released
 * in vmspace_free()
 */

	ldr	r0, [r0, #(P_VMSPACE)]
	bl	_vmspace_free

/* This has to be done here */

	mov	r2, #(UPAGES << PGSHIFT)
	ldr	r0, [sp], #0x0004
	ldr	r1, [r0, #(P_ADDR)]
	ldr	r0, Lkernel_map
	ldr	r0, [r0]
	bl	_kmem_free

/* Paranoia */

	mov	r0, #0x00000000
	ldr	r1, Lcurproc
	str	r0, [r1]

        ldr     r1, Lproc0
	b	switch_search


Lcurrent_spl_level:
	.word	_current_spl_level
 
	.global	_savectx
_savectx:
/*
 * r0 = pcb
 */

/* Push registers.*/

	stmfd	sp!, {r4-r7, lr}

/* Store all the registers in the process's pcb */

	add	r2, r0, #(PCB_R8)
	stmia	r2, {r8-r13}

/* Pull the regs of the stack */

	ldmfd	sp!, {r4-r7, pc}


	.global	_proc_trampoline
_proc_trampoline:
	add	lr, pc, #(trampoline_return - . - 8)
	mov	r0, r5
	mov	r1, sp
	mov	pc, r4

trampoline_return:
/* Kill irq's */

        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAME

	movs	pc, lr			/* Exit */
	
