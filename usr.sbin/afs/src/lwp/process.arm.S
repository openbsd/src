/* $OpenBSD: process.arm.S,v 1.2 2006/11/17 20:54:47 deraadt Exp $ */

/*
 * Copyright (c) 2004 Dale Rahn. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

#include <config.h>

#define FRAME_OFFSET	8	/* XXX */
#define FRAME_SIZE	(52 +(11*4)+FRAME_OFFSET)	/* FP + r4-r14 */

#define TOP_OF_STACK	(0*4)
#define RETURN 		(1*4)


/*
 * savecontext(int (*f)(), struct savearea *save, char *newsp)
 */
	.globl	savecontext
	.type	savecontext%function
savecontext:
	str	r2, [r1]
	ldr	r3, .LPRE_Block
	mov	r2, #1
	str	r2, [r3]
	ldr	r2, [r1]

	sub	sp, sp, #FRAME_SIZE
	add	r3, sp, #FRAME_OFFSET
#if __SOFTFP__
	add	r3, r3, #52
#else
	/* Store fp registers */
	sfm	f4, 4, [r3], #48
	/* Store fpsr */
	rfs	r3
	str	r3, [r3], #0x0004
#endif	/*__SOFTFP__*/
	/* Store integer registers */
        stmia	r3, {r4-r13}

	/* SAVE SP */
	str	sp, [r1, #TOP_OF_STACK]
	str	r14, [r1, #RETURN]
	cmp	r2, #0
        movne	sp, r2
        mov	r15, r0

.LPRE_Block:
	.word	PRE_Block

/*
 * returnto(struct savearea *area)
 */
	.globl	returnto
	.type	returnto%function
returnto:
	ldr	sp, [r0, #TOP_OF_STACK]
	ldr	r14, [r0, #RETURN]
	add	r3, sp, #FRAME_OFFSET
	mov	r1, r3
#if __SOFTFP__
	add	r3, r3, #52
#else
	/* Restore fp registers */
	lfm	f4, 4, [r3], #48
	/* Restore FPSR */
	ldr	r4, [r3], #0x0004
	wfs	r4
#endif	/* __SOFTFP__ */
	/* Restore integer registers */
        ldmia	r3, {r4-r13}

	ldr	r3, .LPRE_Block
	mov	r2, #0
	str	r2, [r3]

	add	sp, sp, #FRAME_SIZE
#ifdef __ARM_26__
	mov	r15, r14
#else
	mov	r15, r14
#endif
