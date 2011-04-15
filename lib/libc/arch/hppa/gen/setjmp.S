/*	$OpenBSD: setjmp.S,v 1.8 2011/04/15 21:39:45 ariane Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#define _LOCORE
#include <machine/frame.h>
#undef _LOCORE

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

ENTRY(sigsetjmp, 64)
	sub,<>	%r0, %arg1, %r0
	b,n	_setjmp
	nop

	stw	%arg1, 44(%arg0)	; last of reserved words

ALTENTRY(setjmp)
	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%arg0, HPPA_FRAME_ARG(0)(%sp)

	copy	%r3, %r1
	copy	%sp, %r3
	stw,ma	%r1, HPPA_FRAME_SIZE(%sp)

	bl	sigblock, %rp
	copy	%r0, %arg0

	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldw,mb	-HPPA_FRAME_SIZE(%sp),%r3
	ldw	HPPA_FRAME_CRP(%sp), %rp
	ldw	HPPA_FRAME_ARG(0)(%sp), %arg0

	stw	%r0, 0(%arg0)		; no onstack
	b	_setjmp$dosaves
	stw	%ret0, 4(%arg0)		; mask

ALTENTRY(_setjmp)
	/* A sigcontext is at the beginning of our jmp_buf. */
	stw	%r0, 0(%arg0)		; no onstack
	stw	%r0, 4(%arg0)		; mask
_setjmp$dosaves
	stw	%r0, 8(%arg0)		; set sc.sc_ps
	stw	%sp, 12(%arg0)		; sc.sc_sp = %sp
	stw	%r0, 16(%arg0)		; sc.sc_fp
	ldo	4(%rp), %r1
	stw	%rp, 20(%arg0)		; sc.sc_pcoqh = %rp
	stw	%r1, 24(%arg0)		; sc.sc_pcoqt = %rp + 4

	/* We store all callee-saved registers after the sigcontext. */
	ldo	48(%arg0), %r1		; offset to after sc
	stwm	%r3, 4(%r1)
	stwm	%r4, 4(%r1)
	stwm	%r5, 4(%r1)
	stwm	%r6, 4(%r1)
	stwm	%r7, 4(%r1)
	stwm	%r8, 4(%r1)
	stwm	%r9, 4(%r1)
	stwm	%r10, 4(%r1)
	stwm	%r11, 4(%r1)
	stwm	%r12, 4(%r1)
	stwm	%r13, 4(%r1)
	stwm	%r14, 4(%r1)
	stwm	%r15, 4(%r1)
	stwm	%r16, 4(%r1)
	stwm	%r17, 4(%r1)
	stwm	%r18, 4(%r1)

	/* Return 0. */
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(_setjmp)

ENTRY(siglongjmp,64)
	/* XXX have to dup this from below */
	ldw	16(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

	ldw	44(%arg0), %arg2
	sub,<>	%r0, %arg2, %r0
	b	_longjmp
	nop

ALTENTRY(longjmp)
	/* XXX have to dup this from below */
	ldw	16(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

	stw	%rp, HPPA_FRAME_CRP(%sp)
	stw	%arg0, HPPA_FRAME_ARG(0)(%sp)
	stw	%arg1, HPPA_FRAME_ARG(1)(%sp)

	copy	%r3, %r1
	copy	%sp, %r3
	stw,ma	%r1, HPPA_FRAME_SIZE(%sp)

	bl	sigsetmask, %rp
	ldw	4(%arg0), %arg0

	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldw,mb	-HPPA_FRAME_SIZE(%sp),%r3
	ldw	HPPA_FRAME_CRP(%sp), %rp
	ldw	HPPA_FRAME_ARG(0)(%sp), %arg0
	b	_longjmp$restores
	ldw	HPPA_FRAME_ARG(1)(%sp), %arg1

ALTENTRY(_longjmp)
	ldw	16(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

_longjmp$restores
	/* restore callee-saved registers */
	ldo	48(%arg0), %r1
	ldwm	4(%r1), %r3
	ldwm	4(%r1), %r4
	ldwm	4(%r1), %r5
	ldwm	4(%r1), %r6
	ldwm	4(%r1), %r7
	ldwm	4(%r1), %r8
	ldwm	4(%r1), %r9
	ldwm	4(%r1), %r10
	ldwm	4(%r1), %r11
	ldwm	4(%r1), %r12
	ldwm	4(%r1), %r13
	ldwm	4(%r1), %r14
	ldwm	4(%r1), %r15
	ldwm	4(%r1), %r16
	ldwm	4(%r1), %r17
	ldwm	4(%r1), %r18

	/* restore the rest */
	ldw	12(%arg0), %sp
	ldw	20(%arg0), %rp		/* check the priv level */
	sub,<>	%arg1, %r0, %r0
	ldo	1(%arg1), %arg1
	bv	%r0(%rp)
	copy	%arg1, %ret0
EXIT(_longjmp)

	.end
