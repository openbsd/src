/* $NetBSD: locore.S,v 1.8 1996/03/20 18:57:58 mark Exp $ */

/*
 * Copyright (C) 1994 Mark Brinicombe
 * Copyright (C) 1994 Brini
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
 *	This product includes software developed by Brini.
 * 4. The name of Brini may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/param.h>
#include <sys/syscall.h>

/* What size shoudl this really be ? It is only used by init_arm() */

#define INIT_ARM_STACK_SIZE	2048

#define PUSHFRAME \
	str	lr, [sp, #-4]!;		/* Push the return address */ \
	sub	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
	stmdb	sp, {r0-r14}^;		/* Push the user mode registers */ \
	sub	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
	mrs	r0, spsr_all;		/* Put the SPSR on the stack */ \
	str	r0, [sp, #-4]!;

#define PULLFRAME \
        ldr     r0, [sp], #0x0004;      /* Get the SPSR from stack */ \
        msr     spsr_all, r0; \
	add	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
        ldmdb   sp, {r0-r14}^;		/* Restore the registers (user mode) */ \
        mov     r0, r0;                 /* NOP for previous instruction */ \
 	add	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
 	ldr	lr, [sp], #0x0004;	/* Pull the return address */

/* register equates */
fp	.req	r11
ip	.req	r12
sp	.req	r13
lr	.req	r14
pc	.req	r15

.text
	.align	0
	.global	start
	.global _C_LABEL(kernel_text)
	_C_LABEL(kernel_text) = start
start:
	add	r1, pc, #(Lstart - . - 8)
	ldmia	r1, { r1, r2, r13 }		/* Set initial stack and */
	sub	r2, r2, r1			/* get zero init data */
	mov	r3, #0

L1:
	str	r3, [r1], #0x0004		/* Zero the bss */
	subs	r2, r2, #4
	bgt	L1

	mov	fp, #0x00000000			/* trace back starts here */
	bl	_initarm			/* Off we go */

/* init arm will return the new stack pointer. */

	mov	sp, r0

/* Debug code */

/*	mov	r1, sp
	add	r0, pc, #(Ltext9 - . - 8)
	bl	_printf
*/

/* Setup an initial trap frame for start_init to use */

	PUSHFRAME

	mov	fp, #0x00000000		/* trace back starts here */
	mov	r0, sp			/* parameter to main is trap frame*/ 

/* Debug */

#if 0
stmfd r13!, {r0-r3}
	ldr	r1, Lproc0
	ldr	r2, [r1]
/*	mov	r1, r0*/
	add	r0, pc, #(Ltext7 - . - 8)
	bl	_printf
ldmfd r13!, {r0-r3}
#endif

	bl	_main			/* Lets light the flame and start her up */

	PULLFRAME			/* Pull the trap frame, now valid */

	movs	pc, lr			/* Exit to user process */

/* Never gets here */

	b	.

Lstart:
	.word	_edata
	.word	_end
	.word	svcstk + INIT_ARM_STACK_SIZE

Lproc0:
	.word	_proc0

/*
Ltext7:
	.asciz	"proc0=%08x %08x\n"
	.align	0


Ltext9:
	.asciz	"proc0 stack at V%08x %08x\n"
	.align	0
*/

/* What size shoudl this really be ? It is only used by init_arm() */

	.bss
svcstk:	.space	INIT_ARM_STACK_SIZE

/*
 * Instructions to copy to the bottom of zero page
 * These are the entry point to the system exception routines
 */

	.text
	.align	0
	.global	_page0, _page0_end
_page0:
	ldr	pc, [pc, #Lreset - . - 8]
	ldr	pc, [pc, #Lundefined - . - 8]
	ldr	pc, [pc, #Lswi - . - 8]
	ldr	pc, [pc, #Labortpre - . - 8]
	ldr	pc, [pc, #Labortdata - . - 8]
	ldr	pc, [pc, #Laddrexc - . - 8]
	ldr	pc, [pc, #Lirq - . - 8]
	ldr	pc, [pc, #Lfiq - . - 8]

Lreset:
	.word	reset_entry
Lundefined:
	.word	undefined_entry
Lswi:
	.word	swi_entry
Labortpre:
	.word	prefetch_abort_entry
Labortdata:
	.word	data_abort_entry
Laddrexc:
	.word	addrexc
Lirq:
	.word	irq_entry

Lfiq:
	.word	fiq
_page0_end =	.

#if 0
reset:
	add	r0, pc, #Wreset - . - 8
	ldmia	r0, {r0, pc}
resetmsg:
	.asciz	"reset"
	.align	0
Wreset:
	.word	resetmsg
	.word	_panic
#endif

reset_entry:
	PUSHFRAME

 	mov	r0, sp			/* Pass the frame to any function */

	bl	_resethandler		/* It's a branch throught zero ! */

	PULLFRAME

	movs	pc, lr			/* Exit */

#if 0
undefined:
#ifndef	KGDB
	add	r0, pc, #Wundefined - . - 8
	ldmia	r0, {r0, pc}
undmsg:
	.asciz	"undefined"
	.align	0
Wundefined:
	.word	undmsg
	.word	_panic
#else
/*
 * lr is the saved pc, and we make space for the saved psr
 */
	stmfd	sp!, {lr, pc}
/*
 * now save all the rest of the registers (r13&r14) are redone later
 */
	stmfd	sp!, {r0-lr}
/*
 * now switch to the old mode to get r13&r14, but disable interrupts
*/
/* stupid gas doesn't yet know about the psr transfers */
	.word	0xe10f3000		@	mrs	r3, cpsr
	.word	0xe14f2000		@	mrs	r2, spsr
	str	r2, [sp, #64]
	orr	r2, r2, #(PSR_FIQ32_MODE|PSR_IRQ32_MODE)
	.word	0xe129f002		@	msr	cpsr, r2
	mov	r0, r13
	mov	r1, r14
	orr	r3, r3, #(PSR_FIQ32_MODE|PSR_IRQ32_MODE)
	.word	0xe129f003		@	msr	cpsr, r3
	str	r0, [sp, #52]
	str	r1, [sp, #56]
	mov	r0, sp
	mov	lr, pc
	ldr	pc, [pc, #Wundefined - . - 8]
	add	sp, sp, #52		/* adjust stack to r13 */
/*
 * set saved psr and get r13&r14 to new (saved) mode first
 */
	ldr	r2, [sp, #12]
	ldr	r0, [sp]
	ldr	r1, [sp, #4]
/* stupid gas doesn't yet know about the psr transfers */
	.word	0xe169f002		@	msr	spsr, r2
	.word	0xe10f3000		@	mrs	r3, cpsr
	orr	r2, r2, #(PSR_FIQ32_MODE|PSR_IRQ32_MODE)
	.word	0xe129f002		@	msr	cpsr, r2
	mov	r13, r0
	mov	r14, r1
	.word	0xe129f003		@	msr	cpsr, r3
	ldmea	sp, {r0-r12}		/* now the rest of the registers */
	add	sp, sp, #16		/* Adjust stack to top */
	ldr	lr, [sp, #-8]		/* get link address */
	movs	pc, lr			/* return */
Wundefined:
	.word	_kgdb_trap_glue
#endif
#endif

addrexc:
	.word	0xe10f1000
	.word	0xe14f2000
	mov	r3, lr
	add	r0, pc, #addrexcmsg - . - 8
	bl	_printf
	b	data_abort_entry

addrexcmsg:
	.asciz	"address exception CPSR=%08x SPSR=%08x lr=%08x\n"
	.align	0
Waddrexc:
	.word	addrexcmsg
	.word	_panic

irq:
	add	r0, pc, #Wirq - . - 8
	ldmia	r0, {r0, pc}
irqmsg:
	.asciz	"irq"
	.align	0
Wirq:
	.word	irqmsg
	.word	_panic

fiq:
	add	r0, pc, #Wfiq - . - 8
	ldmia	r0, {r0, pc}
fiqmsg:
	.asciz	"fiq"
	.align	0
Wfiq:
	.word	fiqmsg
	.word	_panic

	.global	_initmode
_initmode:
/* stupid gas doesn't yet know about the psr transfers */
	mrs	r2, cpsr_all
	msr	cpsr_all, r0
	mov	sp, r1
	msr	cpsr_all, r2
	mov	pc, lr


	.global	_boot0
_boot0:
        mrs     r2, cpsr_all
	bic	r2, r2, #(PSR_MODE)
        orr     r2, r2, #(PSR_SVC32_MODE)
	orr	r2, r2, #(I32_bit | F32_bit)
        msr     cpsr_all, r2

	mov	r0, #0
        mcr     15, 0, r0, c1, c0, 0
 	mov	pc, r0


/* Debug routine to print trace back information from stack */

	.global	_traceback

_traceback:
	stmfd	sp!, {r4, r5, r6, lr}
	mov	r4, r11
	ldr	r5, Ltracebackmin
	ldr	r6, Ltracebackmax

tbloop:
	mov	r1, r4
	ldr	r2, [r4, #-12]
	ldr	r3, [r4, #-8]
	add	r0, pc, #Ltb1 - .  - 8
	bl	_printf

/*	ldr	r1, [r4, #-4]
	ldr	r2, [r4]
	add	r0, pc, #Ltb2 - .  - 8
	bl	_printf*/
	ldr	r0, [r4, #-4]
	ldr	r1, [r4]
	bl	_traceback_sym

	mov	r3, r4
	ldr	r4, [r4, #-12]
	teq	r3, r4
	beq	tbexit
	cmp	r4, r5
	bls	tbexit
	cmp	r4, r6
	bge	tbexit
	teq	r4, #0x00000000
	bne	tbloop

tbexit:
	mov	r0, r4
        ldmfd	sp!, {r4, r5, r6, pc}

Ltracebackmin:
	.word	0xefbfe000

Ltracebackmax:
	.word	0xefc00000

Ltb1:
	.asciz	"traceback: fp=%08x fp->fp=%08x fp->sp=%08x "

Ltb2:
	.asciz	"fp->lr=%08x fp->pc=%08x\n"

	.align	0

/* Debug routine to print trace back information from stack */

	.global	_simpletraceback

_simpletraceback:
	stmfd	sp!, {r4, r5, r6, lr}
	mov	r4, r11
	ldr	r5, Ltracebackmin
	ldr	r6, Ltracebackmax

stbloop:
/*	ldr	r1, [r4, #-4]
	ldr	r2, [r4]
	add	r0, pc, #Ltb2 - .  - 8
	bl	_printf*/
	ldr	r0, [r4, #-4]
	ldr	r1, [r4]
	bl	_traceback_sym

	mov	r3, r4
	ldr	r4, [r4, #-12]
	teq	r3, r4
	beq	stbexit
	cmp	r4, r5
	bls	stbexit
	cmp	r4, r6
	bge	stbexit
	teq	r4, #0x00000000
	bne	stbloop

stbexit:
	mov	r0, r4
        ldmfd	sp!, {r4, r5, r6, pc}


/* Debug routine to print trace back information from stack */

	.global	_irqtraceback

_irqtraceback:
	stmfd	sp!, {r4, r5, r6, lr}
	mov	r4, r0
	mov	r5, r1
	add	r6, r5, #(NBPG)

itbloop:
	mov	r1, r4
	ldr	r2, [r4, #-12]
	ldr	r3, [r4, #-8]
	add	r0, pc, #Ltb1 - .  - 8
	bl	_printf

/*	ldr	r1, [r4, #-4]
	ldr	r2, [r4]
	add	r0, pc, #Ltb2 - .  - 8
	bl	_printf*/
	ldr	r0, [r4, #-4]
	ldr	r1, [r4]
	bl	_traceback_sym

	mov	r3, r4
	ldr	r4, [r4, #-12]
	teq	r3, r4
	beq	itbexit
	cmp	r4, r5
	bls	itbexit
	cmp	r4, r6
	bge	itbexit
	teq	r4, #0x00000000
	bne	itbloop

itbexit:
	mov	r0, r4
        ldmfd	sp!, {r4, r5, r6, pc}


	.global	_user_traceback

_user_traceback:
	stmfd	sp!, {r4, r5, r6, lr}
	mov	r4, r0
	ldr	r5, Lusertracebackmin
	ldr	r6, Lusertracebackmax

usertbloop:
	mov	r1, r4
	ldr	r2, [r4, #-12]
	ldr	r3, [r4, #-8]
	add	r0, pc, #Lusertb1 - .  - 8
	bl	_printf

	ldr	r1, [r4, #-4]
	ldr	r2, [r4]
	add	r0, pc, #Lusertb2 - .  - 8
	bl	_printf

	mov	r3, r4
	ldr	r4, [r4, #-12]
	teq	r3, r4
	beq	usertbexit
	cmp	r4, r5
	bls	tbexit
	cmp	r4, r6
	bge	usertbexit
	teq	r4, #0x00000000
	bne	usertbloop

usertbexit:
	mov	r0, r4
        ldmfd	sp!, {r4, r5, r6, pc}

Lusertracebackmin:
	.word	0x00001000

Lusertracebackmax:
	.word	0xefbfe000

Lusertb1:
	.asciz	"traceback: fp=%08x fp->fp=%08x fp->sp=%08x "

Lusertb2:
	.asciz	"fp->lr=%08x fp->pc=%08x\n"

	.align	0



/*
 * Signal trampoline; copied to top of user stack.
 */
	.global	_sigcode
ENTRY(sigcode)
/*
 * r0-r2 are our signal handler parameters
 * r3 is the handler address
 */

	add	lr, pc, #0			/* Set return address */
	mov	pc, r3				/* Call the handler */

/*
 * Call sig_return with address of the signal context
 * Note: Don't use SIG_SCP as this make have been trashed by the program
 */
	add	r0, sp, #SIGF_SC
	swi	SYS_sigreturn

/* Well if that failed we better exit quick ! */

	add	r0, pc, #Lsigerr - . - 8
	swi	0x1002

	swi	SYS_exit

Lsigerr:
	.asciz "sigreturn syscall failed\n"
	.align	0
        .globl  _esigcode
_esigcode:

/*
 * setjump + longjmp
 */
	.global _setjmp
ENTRY(setjmp)
        stmia	r0, {r4-r14}
        mov	r0, #0x00000000
        mov	r15, r14

	.global _longjmp
ENTRY(longjmp)
        ldmia	r2, {r4-r14}
	mov	r0, #0x00000001
	mov	r15, r14

	.global _esym
_esym:	.word	_end

	.global	_abort
_abort:
	b	_abort
