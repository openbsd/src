/* $NetBSD: exception.S,v 1.5 1996/03/13 21:00:26 mark Exp $ */

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
 * exception.S
 *
 * Low level handlers for exception vectors
 *
 * Created      : 24/09/94
 *
 * Based on kate/display/abort.s
 */

#include <machine/cpu.h>
#include "assym.h"

/*
 * PUSHFRAME - macro to push a trap frame on the stack in the current mode
 * Since the current mode is used, the SVC R14 field is not defined.
 */

#define PUSHFRAME \
	str	lr, [sp, #-4]!;		/* Push the return address */ \
	sub	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
	stmdb	sp, {r0-r14}^;		/* Push the user mode registers */ \
	sub	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
	mrs	r0, spsr_all;		/* Put the SPSR on the stack */ \
	str	r0, [sp, #-4]!;

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
 * PUSHFRAMEINSVC - macro to push a trap frame on the stack in SVC32 mode
 * This should only be used if the processor is not currently in SVC32
 * mode. The processor mode is switched to SVC mode and the trap frame is
 * stored. The SVC R14 field is used to store the previous value of
 * R14 in SVC mode.  
 */

#define PUSHFRAMEINSVC \
	stmdb	sp, {r0-r3};		/* Save 4 registers */ \
	mov	r0, lr;			/* Save xxx32 r14 */ \
	mov	r1, sp;			/* Save xxx32 sp */ \
	mrs	r3, spsr_all;		/* Save xxx32 spsr */ \
	mrs     r2, cpsr_all; 		/* Get the CPSR */ \
	bic     r2, r2, #(PSR_MODE);	/* Fix for SVC mode */ \
	orr     r2, r2, #(PSR_SVC32_MODE); \
	msr     cpsr_all, r2;		/* Punch into SVC mode */ \
	str	r0, [sp, #-4]!;		/* Push return address */ \
	str	lr, [sp, #-4]!;		/* Push SVC r14 */ \
	msr     spsr_all, r3;		/* Restore correct spsr */ \
	ldmdb	r1, {r0-r3};		/* Restore 4 regs from xxx mode */ \
	stmdb	sp, {r0-r14}^;		/* Push the user mode registers */ \
	sub	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
	mrs	r0, spsr_all;		/* Put the SPSR on the stack */ \
	str	r0, [sp, #-4]!

/*
 * PULLFRAMEFROMSVCANDEXIT - macro to pull a trap frame from the stack
 * in SVC32 mode and restore the saved processor mode and PC.
 * This should be used when the SVC R14 register needs to be restored on
 * exit.
 */

#define PULLFRAMEFROMSVCANDEXIT \
        ldr     r0, [sp], #0x0004;	/* Get the SPSR from stack */ \
        msr     spsr_all, r0;		/* restore SPSR */ \
	add	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
        ldmdb   sp, {r0-r14}^;		/* Restore the registers (user mode) */ \
        mov     r0, r0;	  		/* NOP for previous instruction */ \
	ldmia	sp!, {lr, pc}^		/* Restore lr and exit */

sp	.req	r13
lr	.req	r14
pc	.req	r15

	.text	
	.align	0

/* entry point for CPU data abort */

	.global	data_abort_entry

data_abort_entry:
        sub     lr, lr, #0x00000008     /* Adjust the lr */

	PUSHFRAMEINSVC			/* Push trap frame and switch */
					/* to SVC32 mode */

	mov	r0, sp			/* pass the stack pointer as r0 */

	add	lr, pc, #Ldata_abort_return - . - 8
	ldr	r1, Ldata_abort_handler_address
	ldr	pc, [r1]
	
Ldata_abort_return:
#if 0	/* For now only handle ast's after interrupts */
	ldr	r0, [sp]		/* Get the SPSR from stack */
	and	r0, r0, #(PSR_MODE)	/* Test for USR32 mode before the ABT */
	teq	r0, #(PSR_USR32_MODE)
	ldreq	r0, Lastpending		/* Do we have an AST pending ? */
	ldreq	r1, [r0]
	teqeq	r1, #0x00000001
	moveq	r1, #0x00000000		/* Clear it */
	streq	r1, [r0]

	moveq	r0, sp			/* arg 0 = irq frame */
	bleq	_ast			/* call the AST handler */
#endif

/* Kill IRQ's */
        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAMEFROMSVCANDEXIT		/* Restore the trap frame and exit */

Ldata_abort_handler_address:
	.word	_data_abort_handler_address

	.data
	.global	_data_abort_handler_address

_data_abort_handler_address:
	.word	abortdata

	.text
abortdata:
        add     r0, pc, #abortdatamsg - . - 8
	b	_panic

abortdatamsg:
        .asciz  "abortdata"
        .align  0


	.global	prefetch_abort_entry

prefetch_abort_entry:
        sub     lr, lr, #0x00000004     /* Adjust the lr */

	PUSHFRAMEINSVC

 	mov	r0, sp			/* pass the stack pointer as r0 */

	add	lr, pc, #Lprefetch_abort_return - . - 8
	ldr	r1, Lprefetch_abort_handler_address
	ldr	pc, [r1]
	
Lprefetch_abort_return:
#if 0	/* For the moment ast's are only handled by irq handler */
	ldr	r0, [sp]		/* Get the SPSR from stack */
	and	r0, r0, #(PSR_MODE)	/* Test for USR32 mode before the IRQ */
	teq	r0, #(PSR_USR32_MODE)
	ldreq	r0, Lastpending		/* Do we have an AST pending ? */
	ldreq	r1, [r0]
	teqeq	r1, #0x00000001
	moveq	r1, #0x00000000		/* Clear it */
	streq	r1, [r0]

	moveq	r0, sp			/* arg 0 = irq frame */
	bleq	_ast			/* call the AST handler */
#endif

/* Kill IRQ's */
        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAMEFROMSVCANDEXIT

Lprefetch_abort_handler_address:
	.word	_prefetch_abort_handler_address

	.data
	.global	_prefetch_abort_handler_address

_prefetch_abort_handler_address:
	.word	abortprefetch

	.text
abortprefetch:
        add     r0, pc, #abortprefetchmsg - . - 8
	b	_panic

abortprefetchmsg:
        .asciz  "abortprefetch"
        .align  0


/*
 * swi_entry
 *
 * Main entry point for the SWI vector
 */

	.global	swi_entry
swi_entry:
	PUSHFRAME

 	sub	r0, lr, #0x00000004	/* Get the address of the SWI */
	ldr	r4, [r0]		/* Get the instruction */

	bic	r1, r4, #0xff000000	/* Extract the comment field */

	mov	r0, sp			/* Pass the frame to any function */

	bl	_syscall		/* It's a syscall ! */

#if 0	/* Ast's only from the irqhandler .... */
	ldr	r0, Lastpending		/* Do we have an AST pending ? */
	ldr	r1, [r0]
	teq	r1, #0x00000001
	moveq	r1, #0x00000000		/* Clear it */
	streq	r1, [r0]

	moveq	r0, sp			/* arg 0 = irq frame */
	bleq	_ast			/* call the AST handler */
#endif

/* Kill irq's */

        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAME

	movs	pc, lr			/* Exit */

Lastpending:
        .word   _astpending

	.text
	.align	0

/*
 * We indirect the undefined vector via the handler address
 * in the data area.
 * Entry to the undefined handler must look like direct
 * entry from the vector.
 */

	.global	undefined_entry
undefined_entry:
	stmfd	sp!, {r0, r1}
	ldr	r0, Lundefined_handler_indirection
	ldr	r1, [sp], #0x0004
	str	r1, [r0, #0x0000]
	ldr	r1, [sp], #0x0004
	str	r1, [r0, #0x0004]
	ldmia	r0, {r0, r1, pc}

Lundefined_handler_indirection:
	.word	Lundefined_handler_indirection_data

/*
 * assembly bounce code for calling the kernel
 * undefined instruction handler. This uses
 * a standard trap frame and is called in SVC mode.
 */

	.global	_undefinedinstruction_bounce
_undefinedinstruction_bounce:
	PUSHFRAMEINSVC
	mov	r0, sp
	bl	_undefinedinstruction

/* Kill irq's */
        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAMEFROMSVCANDEXIT


	.data
	.align	0

/*
 * Indirection data
 * 2 words use for preserving r0 and r1
 * 3rd word contains the undefined handler address.
 */

Lundefined_handler_indirection_data:
	.word	0
	.word	0

	.global	_undefined_handler_address
_undefined_handler_address:
	.word	_undefinedinstruction_bounce

/* End of exception.S */
