/* $NetBSD: armfpe_glue.S,v 1.6 1996/03/18 19:54:59 mark Exp $ */

/*
 * Copyright (c) 1996 Mark Brinicombe
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * arm_fpe_glue.S
 *
 * Glue code for calling the ARM FPE core code
 *
 * Created      : 21/12/95
 */

#include "assym.h"
#include <machine/cpu.h>

sp	.req	r13
lr	.req	r14
pc	.req	r15

/* Offsets into fpe core for function addresses */

#define ARM_FPE_CORE_ABORT		 0
#define ARM_FPE_CORE_INITWS		 4
#define ARM_FPE_CORE_INITCONTEXT	 8
#define ARM_FPE_CORE_CHANGECONTEXT	12
#define ARM_FPE_CORE_SHUTDOWN		16
#define ARM_FPE_CORE_ACTIVATECONTEXT	20
#define ARM_FPE_CORE_DEACTIVATECONTEXT	24
#define ARM_FPE_CORE_SAVECONTEXT	28
#define ARM_FPE_CORE_LOADCONTEXT	32
#define	ARM_FPE_CORE_DISABLE		36
#define	ARM_FPE_CORE_ENABLE		40

/*
 * Ok Lots of little stubs for calling the fpe core
 * routines from C
 */

	.text
	.align

arm_fpe_header:
	.word	_arm_fpe_mod

	.global	_arm_fpe_core_disable
_arm_fpe_core_disable:
	stmfd	sp!, {r0-r7, lr}
	ldr	r0, [pc, #arm_fpe_header - . - 8]
	ldr	r0, [r0, #ARM_FPE_CORE_DISABLE]

	add	lr, pc, #L1 - . - 8
	mov	pc, r0
L1:
	ldmfd	sp!, {r0-r7, pc}


	.global	_arm_fpe_core_enable
_arm_fpe_core_enable:
	stmfd	sp!, {r0-r7, lr}
	ldr	r0, [pc, #arm_fpe_header - . - 8]
	ldr	r0, [r0, #ARM_FPE_CORE_ENABLE]

	add	lr, pc, #L2 - . - 8
	mov	pc, r0
L2:
	ldmfd	sp!, {r0-r7, pc}


	.global	_arm_fpe_core_initws
_arm_fpe_core_initws:
	stmfd	sp!, {r10, lr}
	mov	r10, r0
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_INITWS]

	add	lr, pc, #L3 - . - 8
	mov	pc, r3
L3:
	ldmfd	sp!, {r10, pc}


	.global	_arm_fpe_core_abort
_arm_fpe_core_abort:
	stmfd	sp!, {r1-r7, r10, lr}
	mov	r10, r0
	mov	r0, r1
	mov	r1, r2
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_ABORT]

	add	lr, pc, #L4 - . - 8
	mov	pc, r3
L4:
	ldmfd	sp!, {r1-r7, r10, pc}


/* Only needs to preserve r10 */

	.global	_arm_fpe_core_initcontext
_arm_fpe_core_initcontext:
	stmfd	sp!, {r0-r7, r10, lr}
	mov	r10, r0
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_INITCONTEXT]

	add	lr, pc, #L5 - . - 8
	mov	pc, r3
L5:
	ldmfd	sp!, {r0-r7, r10, pc}


/* Only needs to preserve r10 */

	.global	_arm_fpe_core_changecontext
_arm_fpe_core_changecontext:
	stmfd	sp!, {r1-r7, r10, lr}
	mov	r10, r0
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_CHANGECONTEXT]

	add	lr, pc, #L6 - . - 8
	mov	pc, r3
L6:
	ldmfd	sp!, {r1-r7, r10, pc}


/* All regs preerved */

	.global	_arm_fpe_core_shutdown
_arm_fpe_core_shutdown:
	stmfd	sp!, {r0-r7, r10, lr}
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_SHUTDOWN]

	add	lr, pc, #L7 - . - 8
	mov	pc, r3
L7:
	ldmfd	sp!, {r0-r7, r10, pc}


/* Preserve r10 */
	.global	_arm_fpe_core_savecontext
_arm_fpe_core_savecontext:
	stmfd	sp!, {r1-r7, r10, lr}
	mov	r10, r0
	mov	r0, r1
	mov	r1, r2
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_SAVECONTEXT]

	add	lr, pc, #L8 - . - 8
	mov	pc, r3
L8:
	ldmfd	sp!, {r1-r7, r10, pc}


/* Preserve r10 */

	.global	_arm_fpe_core_loadcontext
_arm_fpe_core_loadcontext:
	stmfd	sp!, {r0-r7, r10, lr}
	mov	r10, r0
	mov	r0, r1
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_LOADCONTEXT]

	add	lr, pc, #L9 - . - 8
	mov	pc, r3
L9:
	ldmfd	sp!, {r0-r7, r10, pc}


/* Only needs to preserve r10 */

	.global	_arm_fpe_core_activatecontext
_arm_fpe_core_activatecontext:
	stmfd	sp!, {r0-r7, r10, lr}
	mov	r10, r0
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_ACTIVATECONTEXT]

	add	lr, pc, #L10 - . - 8
	mov	pc, r3
L10:
	ldmfd	sp!, {r0-r7, r10, pc}


/* Only needs to preserve r10 */

	.global	_arm_fpe_core_deactivatecontext
_arm_fpe_core_deactivatecontext:
	stmfd	sp!, {r1-r7, r10, lr}
	ldr	r3, [pc, #arm_fpe_header - . - 8]
	ldr	r3, [r3, #ARM_FPE_CORE_DEACTIVATECONTEXT]

	add	lr, pc, #L11 - . - 8
	mov	pc, r3
L11:
	ldmfd	sp!, {r1-r7, r10, pc}


/* Simple call back function that panics */

	.global	_arm_fpe_panic
_arm_fpe_panic:
	adr	r0, fpe_panic_text
	b	_panic

fpe_panic_text:
	.asciz	"armfpe: we are panicing"
	.align	0


/*
 * Call back routine from FPE on completion of an instruction
 */

	.global	_arm_fpe_post_proc_glue
_arm_fpe_post_proc_glue:
	stmfd	sp!, {r0-r6, r10-r12, lr}

/* This could be optimised as we are going from UND32->SVC32 mode */

        mrs     r4, cpsr_all
	bic	r3, r4, #(PSR_MODE)
        orr     r3, r3, #(PSR_SVC32_MODE)
        msr     cpsr_all, r3

	mov	r0, r12

/* Reserve a trapframe on the SVC stack */

	sub	sp, sp, #(TRAPFRAMESIZE)
	mov	r1, sp

	ldr	r2, [r0, #-0x0008]	/* Copy spsr */
	str	r2, [r1, #0x0000]

	ldr	r2, [r0, #0x0000]	/* Copy r0 */
	str	r2, [r1, #0x0004]
	ldr	r2, [r0, #0x0004]
	str	r2, [r1, #0x0008]
	ldr	r2, [r0, #0x0008]
	str	r2, [r1, #0x000c]
	ldr	r2, [r0, #0x000c]
	str	r2, [r1, #0x0010]
	ldr	r2, [r0, #0x0010]
	str	r2, [r1, #0x0014]
	ldr	r2, [r0, #0x0014]
	str	r2, [r1, #0x0018]
	ldr	r2, [r0, #0x0018]
	str	r2, [r1, #0x001c]
	ldr	r2, [r0, #0x001c]
	str	r2, [r1, #0x0020]
	ldr	r2, [r0, #0x0020]
	str	r2, [r1, #0x0024]
	ldr	r2, [r0, #0x0024]
	str	r2, [r1, #0x0028]
	ldr	r2, [r0, #0x0028]
	str	r2, [r1, #0x002c]
	ldr	r2, [r0, #0x002c]
	str	r2, [r1, #0x0030]
	ldr	r2, [r0, #0x0030]	/* Copy r12 */
	str	r2, [r1, #0x0034]
	ldr	r2, [r0, #0x0034]	/* Copy usr r13 */
	str	r2, [r1, #0x0038]
	ldr	r2, [r0, #0x0038]	/* Copy usr r14 */
	str	r2, [r1, #0x003c]
	ldr	r2, [r0, #0x003c]	/* Copy old pc */
	str	r2, [r1, #0x0044]

	str	r14, [r1, #0x0040]	/* SVC r14 */

/*
 * OK Question Time ...
 *
 * Do I need to save SVC r14 ? 
 * It only needs saving if this routine can interrupt something already
 * running in SVC mode. Since FP is only valid from USR32 mode this
 * should not happen.
 */

	mov	r5, r14
	mov	r6, r12

/* More optimisation ... Need to code a assembly version of userret() */

	bl	_arm_fpe_postproc

/* Release the trapframe on the SVC stack */

	ldr	r2, [sp, #0x0000]	/* Copy spsr */
	str	r2, [r6, #-0x0008]

	ldr	r2, [sp, #0x0004]	/* Copy r0 */
	str	r2, [r6, #0x0000]
	ldr	r2, [sp, #0x0008]	/* Copy r1 */
	str	r2, [r6, #0x0004]
	ldr	r2, [sp, #0x000c]	/* Copy r2 */
	str	r2, [r6, #0x0008]
	ldr	r2, [sp, #0x0010]	/* Copy r3 */
	str	r2, [r6, #0x000c]
	ldr	r2, [sp, #0x0014]	/* Copy r4 */
	str	r2, [r6, #0x0010]
	ldr	r2, [sp, #0x0018]	/* Copy r5 */
	str	r2, [r6, #0x0014]
	ldr	r2, [sp, #0x001c]	/* Copy r6 */
	str	r2, [r6, #0x0018]
	ldr	r2, [sp, #0x0020]	/* Copy r7 */
	str	r2, [r6, #0x001c]
	ldr	r2, [sp, #0x0024]	/* Copy r8 */
	str	r2, [r6, #0x0020]
	ldr	r2, [sp, #0x0028]	/* Copy r9 */
	str	r2, [r6, #0x0024]
	ldr	r2, [sp, #0x002c]	/* Copy r10 */
	str	r2, [r6, #0x0028]
	ldr	r2, [sp, #0x0030]	/* Copy r11 */
	str	r2, [r6, #0x002c]
	ldr	r2, [sp, #0x0034]	/* Copy r12 */
	str	r2, [r6, #0x0030]
	ldr	r2, [sp, #0x0038]	/* Copy usr r13 */
	str	r2, [r6, #0x0034]
	ldr	r2, [sp, #0x003c]	/* Copy usr r14 */
	str	r2, [r6, #0x0038]
	ldr	r2, [sp, #0x0044]	/* Copy pc */
	str	r2, [r6, #0x003c]

	add	sp, sp, #(TRAPFRAMESIZE)

	mov	r14, r5

	msr	cpsr_all, r4

	ldmfd	sp!, {r0-r6, r10-r12, pc}


/*
 * Call back routine from FPE when the an exception occurs
 */

	.global	_arm_fpe_exception_glue
_arm_fpe_exception_glue:
	stmfd	sp!, {r0-r5, r10-r12, lr}

/* This could be optimised as we are going from UND32->SVC32 mode */

        mrs     r4, cpsr_all
	bic	r3, r4, #(PSR_MODE)
        orr     r3, r3, #(PSR_SVC32_MODE)
        msr     cpsr_all, r3

	ldr	r1, [r12, #15*4]
	mov	r2, r12

	mov	r5, r14

	bl	_arm_fpe_exception

	mov	r14, r5

	msr	cpsr_all, r4

	ldmfd	sp!, {r0-r5, r10-r12, lr}

/* Now pull the original trapframe that the FPE pushed off the stack */

	ldmdb	r12, {r0, r1}

	msr	cpsr_all, r1
	msr	spsr_all, r0

	mov	sp, r12

	ldmia	sp, {r0-r14}^
	mov	r0, r0
	add	sp, sp, #15*4
	ldmfd	sp!, {pc}^

	.global _fpe_nexthandler
_fpe_nexthandler:
	.word	_undefinedinstruction_bounce
