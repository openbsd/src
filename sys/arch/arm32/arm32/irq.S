/* $NetBSD: irq.S,v 1.4 1996/03/27 20:42:53 mark Exp $ */

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
 * irq.S
 *
 * Low level irq and fiq handlers
 *
 * Created      : 27/09/94
 */

#include "assym.h"
#include <machine/cpu.h>
#include <machine/iomd.h>

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
	mov	r0, r0;			/* NOP for previous instruction */ \
	add	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
	ldr	lr, [sp], #0x0004;	/* Pull the return address */

#define PULLFRAMEANDEXIT \
        ldr     r0, [sp], #0x0004;      /* Get the SPSR from stack */ \
        msr     spsr_all, r0; \
	add	sp, sp, #(4*15);	/* Adjust the stack pointer */ \
        ldmdb   sp, {r0-r14}^;		/* Restore the registers (user mode) */ \
	mov	r0, r0;			/* NOP for previous instruction */ \
	add	sp, sp, #0x00000004;	/* Skip SVC R14 */ \
	ldmia	sp!, {pc}^		/* Pull the return address */

sp	.req	r13
lr	.req	r14
pc	.req	r15

.text

/*
 *
 * irq_entry
 *
 * Main entry point for the IRQ vector
 *
 * This function reads the irq request bits in the IOMD registers
 * IRQRQA, IRQRQB and DMARQ
 * It then calls an installed handler for each bit that is set.
 * The function stray_irqhandler is called if a handler is not defined
 * for a particular interrupt.
 * If a interrupt handler is found then it is called with r0 containing
 * the argument defined in the handler structure. If the field ih_arg
 * is zero then a pointer to the IRQ frame on the stack is passed instead.
 */

Ldisabled_mask:
	.word	_disabled_mask

Lcurrent_spl_level:	
	.word	_current_spl_level

	.global	irq_entry

/*
 * Regsister usage
 *
 *  r6  - Address of current handler
 *  r7  - Pointer to handler pointer list
 *  r8  - Current IRQ requests.
 *  r9  - Used to count through possible IRQ bits.
 *  r10 - Base address of IOMD
 */

irq_entry:
	sub	lr, lr, #0x00000004	/* Adjust the lr */

	PUSHFRAME

/* Raise the spl level and re-enable interrupts */

#if 0
	ldr	r1, Lcurrent_spl_level
	ldr	r0, [r1]
	add	r0, r0, #1
	bl	_raisespl
#endif
#if 0
        mrs     r0, cpsr_all		/* Enable IRQ's */
	bic	r0, r0, #I32_bit
	msr	cpsr_all, r0
#endif

/*
 * If we did not raise the spl level high enough we will get another
 * interrupt here.
 */

        mov     r11, #0x00000000
 	mov	r10, #(IOMD_BASE)	/* Point to the IOMD */
	ldrb	r8, [r10, #(IOMD_IRQRQA - IOMD_BASE)]	/* Get IRQ request A */
/*	strb	r8, [r10, #(IOMD_IRQRQA - IOMD_BASE)]*/
	ldrb	r9, [r10, #(IOMD_IRQRQB - IOMD_BASE)]	/* Get IRQ request B */
	orr	r8, r8, r9, lsl #8
	ldrb	r9, [r10, #(IOMD_DMARQ - IOMD_BASE)]	/* Get DMA Request */
	orr	r8, r8, r9, lsl #16

/*#if 0*/
	mov	r0, #0x7d		/* Clear IOMD IRQ bits */
	strb	r0, [r10, #(IOMD_IRQRQA - IOMD_BASE)]
/*#endif*/

/*	ldr	r1, Ldisabled_mask
	ldr	r0, [r1]
	orr	r0, r0, r8
	str	r0, [r1]

	bl	_irq_setmasks*/

#if 0
        mrs     r0, cpsr_all		/* Enable IRQ's */
	bic	r0, r0, #I32_bit
	msr	cpsr_all, r0
#endif
	mov	r0, sp
	mov	r1, r8
	bl	_validate_irq_address

	ldr	r7, [pc, #irqhandlers - . - 8]
        mov	r9, #0x00000001

	stmfd	sp!, {r8}

irqloop:
	tst	r8, r9			/* Is a bit set ? */

	beq	nextirq			/* No ? try next bit */

	ldr	r6, [r7]		/* Get address of first handler structure */

	teq	r6, #0x00000000		/* Do we have a handler */
	moveq	r0, r8			/* IRQ requests as arg 0 */
	beq	_stray_irqhandler	/* call special handler */

        ldr	r0, Lcnt
	ldr	r1, [r0, #(V_INTR)]
	add	r1, r1, #0x00000001
	str	r1, [r0, #(V_INTR)]

irqchainloop:
	add	lr, pc, #nextinchain - . - 8	/* return address */

#ifdef IRQSTATS
        ldr	r0, Lintrcnt
	ldr	r1, [r6, #(IH_NUM)]

	add	r0, r0, r1, lsl #2
	ldr	r1, [r0]
	add	r1, r1, #0x00000001
	str	r1, [r0]
#endif

	ldr	r0, [r6, #(IH_ARG)]	/* Get argument pointer */
	teq	r0, #0x00000000		/* If arg is zero pass stack frame */
	addeq	r0, sp, #4			/* ... stack frame */
	ldr	pc, [r6, #(IH_FUNC)]	/* Call handler */

nextinchain:
/*	ldr	r1, Ldisabled_mask
	ldr	r2, [r1]
	bic	r2, r2, r9
	str	r2, [r1]
	bl	_irq_setmasks*/

	teq	r0, #0x00000001		/* Was the irq serviced ? */
	beq	nextirq

	ldr	r6, [r6, #(IH_NEXT)]
	teq	r6, #0x00000000
/*	bne	irqchainloop*/

nextirq:
	add	r7, r7, #0x00000004	/* update pointer to handlers */
	mov	r9, r9, lsl #1		/* move on to next bit */
	teq	r9, #(1 << 24)		/* done the last bit ? */
	bne	irqloop			/* no - loop back. */

	ldmfd	sp!, {r8}


#if 0
        mrs     r0, cpsr_all		/* Enable IRQ's */
	orr	r0, r0, #I32_bit
	msr	cpsr_all, r0
#endif

	bl	_dosoftints		/* Handle the soft interrupts */

/* Disable interrupts and lower the spl level */

#if 0
        mrs     r0, cpsr_all		/* Enable IRQ's */
	orr	r0, r0, #I32_bit
	msr	cpsr_all, r0
#endif
#if 0
	ldr	r1, Lcurrent_spl_level
	ldr	r0, [r1]
	sub	r0, r0, #1
	bl	_lowerspl
#endif

	ldr	r0, [sp]		/* Get the SPSR from stack */

	and	r0, r0, #(PSR_MODE)	/* Test for USR32 mode before the IRQ */
	teq	r0, #(PSR_USR32_MODE)
	ldreq	r0, Lastpending		/* Do we have an AST pending ? */
	ldreq	r1, [r0]
	teqeq	r1, #0x00000001
	moveq	r1, #0x00000000		/* Clear it */
	streq	r1, [r0]

	moveq	r0, sp			/* arg 0 = irq frame */
	beq	_irqast			/* exit via the AST handler */

	PULLFRAME

	movs	pc, lr			/* Exit */


	.global	_irqast
_irqast:
/* Punch into SVC32 mode (R0 points to the irqframe) */
/* We can trash all the registers we like :-) */

/* Debug message */

/*	stmfd	sp!, {r0-r3}
	add	r0, pc, #Lirqtext0 - . - 8
	bl	_printf
	ldmfd	sp!, {r0-r3}*/

        mrs     r2, cpsr_all
	tst	r2, #(I32_bit)
	bne	Lis

        orr     r2, r2, #(I32_bit)
        msr     cpsr_all, r2

	stmfd	sp!, {r0-r3, lr}
	add	r0, pc, #Lirqtext0 - . - 8
	bl	_printf
	ldmfd	sp!, {r0-r3, lr}
Lis:
	add	sp, sp, #72	/* Correct IRQ32 sp */

/*
 * We have now put IRQ mode back correct so we never need to return to
 * IRQ mode we can just exit via SVC mode. We must copy the trap frame
 * which still lies on the IRQ stack over to the SVC stack.
 */

/* Punch into SVC 32 mode (IRQ's still disabled) */

	mrs     r2, cpsr_all
	bic     r2, r2, #(PSR_MODE)
	eor     r2, r2, #(PSR_SVC32_MODE)
	orr	r2, r2, #(I32_bit)	/* Overkill */
	msr     cpsr_all, r2

	sub	sp, sp, #72	/* Correct SVC32 sp */
	mov	r12, sp

/* r0  points to the trap frame on the IRQ stack (SP corrected) */
/* r12 points to the trap frame on the SVC stack */

	ldmia	r0!, {r1-r9} 	/* Pull 9 regs off the IRQ stack */
	stmia	r12!, {r1-r9}	/* Store on the SVC stack */
	ldmia	r0!, {r1-r9} 	/* Pull 9 regs off the IRQ stack */
	stmia	r12!, {r1-r9}	/* Store on the SVC stack */

/* Ok the IRQ trapframe is now the SVC trapframe */

/* IRQ's could be enabled here */

/* Debug message */

/*	stmfd	sp!, {r0-r3}
	add	r0, pc, #Lirqtext1 - . - 8
	bl	_printf
	ldmfd	sp!, {r0-r3}
*/
/* r0 points to trap frame on entry to ast() */

	mov	r0, sp
	stmfd	sp!, {lr}	/* Is this needed ? */

	bl	_ast

	ldmfd	sp!, {lr}	/* Is this needed ? */

/* Remember the address of the trap frame */

/*	stmfd	sp!, {r0-r3}
	add	r0, pc, #Lirqtext2 - . - 8
	bl	_printf
	ldmfd	sp!, {r0-r3}
*/

/* Pull the frame from the SVC stack and return */

/*	mov	lr, #0*/

/* Kill IRQ's incase ast as somehow re-enabled them ... */

        mrs     r0, cpsr_all
        orr     r0, r0, #(I32_bit)
        msr     cpsr_all, r0

	PULLFRAMEANDEXIT

	mov	r4, #0x000000A0
	mov	pc, #0x00000000

Lirqtext0:
	.asciz	"irqs enabled during ast\n"
	.align	0

Lirqtext1:
	.asciz	"irqframe copied to SVC stack\n"
	.align	0

Lirqtext2:
	.asciz	"irqframe restored from SVC stack\n"
	.align	0

Lspl_mask:
	.word	_spl_mask

Lcurrent_mask:
	.word	_current_mask

	.global	_irq_setmasks

_irq_setmasks:
	ldr	r1, Lcurrent_mask
	ldr	r1, [r1]
	ldr	r2, Lspl_mask
	ldr	r2, [r2]
	and	r1, r1, r2
	ldr	r2, Ldisabled_mask
	ldr	r2, [r2]
	bic	r1, r1, r2

 	mov	r0, #(IOMD_BASE)	/* Point to the IOMD */
	strb	r1, [r0, #(IOMD_IRQMSKA - IOMD_BASE)]	/* Set IRQ mask A */
	mov	r1, r1, lsr #8
	strb	r1, [r0, #(IOMD_IRQMSKB - IOMD_BASE)]	/* Set IRQ mask B */
	mov	r1, r1, lsr #8
	strb	r1, [r0, #(IOMD_DMAMSK - IOMD_BASE)]	/* Set DMA mask */
	mov	pc, r14


Lcnt:
	.word	_cnt

Lintrcnt:
	.word	_intrcnt


irqhandlers:
	.word	_irqhandlers		/* Pointer to array of irqhandlers */

Lastpending:
	.word	_astpending

#ifdef IRQSTATS
/* These symbols are used by vmstat */

	.text
	.global	__intrnames
__intrnames:
	.word	_intrnames

	.data

        .globl  _intrnames, _eintrnames, _intrcnt, _eintrcnt
_intrnames:
	.asciz	"interrupt  0 "
	.asciz	"softnet      "	/* reserved0 */
	.asciz	"interrupt  2 "
	.asciz	"interrupt  3 "
	.asciz	"interrupt  4 "
	.asciz	"interrupt  5 "
	.asciz	"interrupt  6 "
	.asciz	"softclock    "	/* reserved1 */
	.asciz	"reserved 2   "
	.asciz	"interrupt  9 "
	.asciz	"interrupt 10 "
	.asciz	"interrupt 11 "
	.asciz	"interrupt 12 "
	.asciz	"interrupt 13 "
	.asciz	"interrupt 14 "
	.asciz	"interrupt 15 "
	.asciz	"dma channel 0"
	.asciz	"dma channel 1"
	.asciz	"dma channel 2"
	.asciz	"dma channel 3"
	.asciz	"interrupt 20 "
	.asciz	"interrupt 21 "
	.asciz	"reserved 3   "
	.asciz	"reserved 4   "
	.asciz	"exp card 0   "
	.asciz	"exp card 1   "
	.asciz	"exp card 2   "
	.asciz	"exp card 3   "
	.asciz	"exp card 4   "
	.asciz	"exp card 5   "
	.asciz	"exp card 6   "
	.asciz	"exp card 7   "
_eintrnames:

	.bss
	.align	0
_intrcnt:
	.space	32*4
_eintrcnt:

#else
/* Dummy entries to keep vmstat happy */

	.text
        .globl  _intrnames, _eintrnames, _intrcnt, _eintrcnt
_intrnames:
	.long	0
_eintrnames:

_intrcnt:
	.long	0
_eintrcnt:
#endif

/* FIQ code */

	.text
	.align	0
	.global	_fiq_setregs		/* Sets up the FIQ handler */

_fiq_setregs:
        mrs	r2, cpsr_all
	mov	r3, r2
	bic	r2, r2, #(PSR_MODE)
	orr	r2, r2, #(PSR_FIQ32_MODE)
        msr	cpsr_all, r2

	ldr	r8,  [r0, #0x000c]	/* Update FIQ registers*/
	ldr	r9,  [r0, #0x0010]
	ldr	r10, [r0, #0x0014]
	ldr	r11, [r0, #0x0018]
	ldr	r12, [r0, #0x001c]
	ldr	r13, [r0, #0x0020]

        msr	cpsr_all, r3		/* Back to old mode */

	mov	pc, lr			/* Exit */

/* End of irq.S */
