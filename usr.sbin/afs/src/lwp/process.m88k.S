/*	$OpenBSD: process.m88k.S,v 1.4 2004/09/15 05:55:46 miod Exp $	*/

/*
 * Copyright (c) 2004, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

#include <config.h>

.globl	_C_LABEL(PRE_Block)

/*
 * savecontext(int (*f)(), struct savearea *a1, char *newsp)
 */
ENTRY(savecontext)
	/* save all registers but r0, r2, r3 and r31 on stack - we could
	   skip r4 but saving it allows us to use it as a temporary below. */
	subu	r31, r31, 4 * 30
	st.d	r4,  r31, 4 * 0
	st.d	r6,  r31, 4 * 2
	st.d	r8,  r31, 4 * 4
	st.d	r10, r31, 4 * 6
	st.d	r12, r31, 4 * 8
	st.d	r14, r31, 4 * 10
	st.d	r16, r31, 4 * 12
	st.d	r18, r31, 4 * 14
	st.d	r20, r31, 4 * 16
	st.d	r22, r31, 4 * 18
	st.d	r24, r31, 4 * 20
	st.d	r26, r31, 4 * 22
	st.d	r28, r31, 4 * 24
	st	r30, r31, 4 * 26
	st	r1,  r31, 4 * 27
	fldcr	r4,  fcr62
	fldcr	r5,  fcr63
	st.d	r4,  r31, 4 * 28

	/* set preemption lock */
	or.u	r4,  r0,  hi16(_C_LABEL(PRE_Block))
	set	r1,  r0,  1<0>
	st.b	r1,  r4,  lo16(_C_LABEL(PRE_Block))
	ld.d	r4,  r31, 4 * 0

	/* save stack pointer, and fetch the new one if non NULL */
	bcnd.n	eq0, r4,  1f
	 st	r31, r3,  0
	or	r31, r4,  r0
1:

	/* invoke function */
	jsr	r2

	/* should not return, invoke abort() if this happens */
	bsr	_C_LABEL(abort)
	/* NOTREACHED */
	
/*
 * returnto(struct savearea *a2)
*/
ENTRY(returnto)
	/* pick stack */
	ld	r31, r2,  0

	/* restore registers */
	ld.d	r4,  r31, 4 * 28
	fstcr	r4,  fcr62
	fstcr	r5,  fcr63
	ld.d	r4,  r31, 4 * 0
	ld.d	r6,  r31, 4 * 2
	ld.d	r8,  r31, 4 * 4
	ld.d	r10, r31, 4 * 6
	ld.d	r12, r31, 4 * 8
	ld.d	r14, r31, 4 * 10
	ld.d	r16, r31, 4 * 12
	ld.d	r18, r31, 4 * 14
	ld.d	r20, r31, 4 * 16
	ld.d	r22, r31, 4 * 18
	ld.d	r24, r31, 4 * 20
	ld.d	r26, r31, 4 * 22
	ld.d	r28, r31, 4 * 24
	ld	r30, r31, 4 * 26
	ld	r1,  r31, 4 * 27
	addu	r31, r31, 4 * 30

	/* clear preemption lock and return */
	or.u	r2,  r0,  hi16(_C_LABEL(PRE_Block))
	jmp.n	r1
	 st.b	r0,  r2,  lo16(_C_LABEL(PRE_Block))
