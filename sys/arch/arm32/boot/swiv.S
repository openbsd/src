#include "regs.h"

	.text

SWIReturnInst:
	ldr	pc, [sp, #0*4]

	.global	_swix
_swix:
	orr	r0, r0, #0x20000

	.global	_swi
_swi:

/*
 * Construct a stack frame that looks something like this:
 *       returnval
 *       LDMIA   r12!, {r0..rn}
 *       SWI     xxxxxx
 *       LDR     pc, [sp]
 *       saved r4-r11,lr
 *       saved r1
 *       saved input values (r2...rn)
 */

	mov	pc, lr

	stmfd	sp!, {r1-r3}		/* Save r1 and put 1st two variadic args on stack */
	bic	r2, r0, #0xff000000
	orr	r2, r2, #0xef000000	/* Construct SWI instruction */
	adr	r0, SWIReturn
	tst	r1, #0x20000		/* bit for write flags */
	adrne	r0, SWIReturnFlags
	bic	r1, r1, #0xff000000	/* Construct LDMIA R12!, {regs} instruction, if */
	bics	r1, r1, #0x00ff0000	/* {regs} = {} (IE no input regs) we must not */
	orrne	r1, r1, #0xe8000000	/* use an LDMIA R12!, {} instruction as this is an */
	orrne	r1, r1, #0x00bc0000	/* invalid instruction, we use a suitable NOP instead */
	moveq	r1, #0			/* 0 = opcode for ANDEQ r0, r0, r0 (a suitable NOP) */
	ldr	r3, SWIReturnInst
	stmfd	sp!, {r0-r9, r11, lr}	/* Save regs and set up SWI call routine (in R0-R3) */
	add	r12, sp, #(12+1)*4	/* Point R12 at input regs on stack. */
	add	pc, sp, #4		/* Call routine on stack */
SWIReturnFlags:
	ldr	r11, [r12], #4
	str	pc, [r11]		/* write flags */
SWIReturn:
	ldr	lr, [sp, #(12+0)*4]	/* Fetch reg mask again */
	movs	lr, lr, asl #1		/* Shift out setting C if R0 to be written, N */
	ldrcs	r11, [r12], #4		/* if R1 to be written. */
	strcs	r0, [r11]
	ldrmi	r11, [r12], #4
	strmi	r1, [r11]
	movs	lr, lr, asl #2		/* Shift 2 bits each time for the next 2 regs */
	ldrcs	r11, [r12], #4
	strcs	r2, [r11]
	ldrmi	r11, [r12], #4
	strmi	r3, [r11]
	movs	lr, lr, asl #2
	ldrcs	r11, [r12], #4
	strcs	r4, [r11]
	ldrmi	r11, [r12], #4
	strmi	r5, [r11]
	movs	lr, lr, asl #2
	ldrcs	r11, [r12], #4
	strcs	r6, [r11]
	ldrmi	r11, [r12], #4
	strmi	r7, [r11]
	movs	lr, lr, asl #2
	ldrcs	r11, [r12], #4
	strcs	r8, [r11]
	ldrmi	r11, [r12], #4
	strmi	r9, [r11]
	ldr	r1, [sp, #2*4]
	tst	r1, #0x20000		/* X-bit clear */
	cmpeq	pc, #0x80000000		/* SET V flag if so, so R0 not cleared */
	movvc	r0, #0			/* Clear R0 if no error (or X-bit clear) */
	add	sp, sp, #4*4		/* Drop SWI call routine */
	ldmia	sp!, {r4-r9,r11,lr}
	add	sp, sp, #3*4		/* Drop saved R1 and 1st two variadic args. */
	movs	pc, lr
