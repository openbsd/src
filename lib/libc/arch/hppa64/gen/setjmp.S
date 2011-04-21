/*	$OpenBSD: setjmp.S,v 1.3 2011/04/21 14:02:42 jsing Exp $	*/

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

ENTRY(sigsetjmp, 128)
	sub,<>	%r0, %arg1, %r0
	b,n	_setjmp
	nop

	std	%arg1, 88(%arg0)	; last of reserved words

ALTENTRY(setjmp)
	std	%rp, HPPA_FRAME_RP(%sp)
	std	%arg0, HPPA_FRAME_ARG(0)(%sp)

	copy	%r3, %r1
	copy	%sp, %r3
	std,ma	%r1, HPPA_FRAME_SIZE(%sp)

	bl	sigblock, %rp
	copy	%r0, %arg0

	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldd,mb	-HPPA_FRAME_SIZE(%sp),%r3
	ldd	HPPA_FRAME_RP(%sp), %rp
	ldd	HPPA_FRAME_ARG(0)(%sp), %arg0

	std	%r0, 0(%arg0)		; no onstack
	b	_setjmp$dosaves
	std	%ret0, 8(%arg0)		; mask

ALTENTRY(_setjmp)
	/* A sigcontext is at the beginning of our jmp_buf. */
	std	%r0, 0(%arg0)		; no onstack
	std	%r0, 8(%arg0)		; mask
_setjmp$dosaves
	std	%r0, 16(%arg0)		; set sc.sc_ps
	std	%sp, 24(%arg0)		; sc.sc_sp = %sp
	std	%r0, 32(%arg0)		; sc.sc_fp
	ldo	4(%rp), %r1
	std	%rp, 40(%arg0)		; sc.sc_pcoqh = %rp
	std	%r1, 48(%arg0)		; sc.sc_pcoqt = %rp + 4

	/* We store all callee-saved registers after the sigcontext. */
	ldo	96(%arg0), %r1		; offset to after sc
	std,ma	%r3, 8(%r1)
	std,ma	%r4, 8(%r1)
	std,ma	%r5, 8(%r1)
	std,ma	%r6, 8(%r1)
	std,ma	%r7, 8(%r1)
	std,ma	%r8, 8(%r1)
	std,ma	%r9, 8(%r1)
	std,ma	%r10, 8(%r1)
	std,ma	%r11, 8(%r1)
	std,ma	%r12, 8(%r1)
	std,ma	%r13, 8(%r1)
	std,ma	%r14, 8(%r1)
	std,ma	%r15, 8(%r1)
	std,ma	%r16, 8(%r1)
	std,ma	%r17, 8(%r1)
	std,ma	%r18, 8(%r1)

	/* Return 0. */
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(_setjmp)

ENTRY(siglongjmp,64)
	/* XXX have to dup this from below */
	sub,>>	%sp, %arg0, %r0		/* botch if jbuf was on stack */
	bl,n	longjmperror, %rp
	nop
	ldd	32(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

	ldd	88(%arg0), %arg2
	sub,<>	%r0, %arg2, %r0
	b	_longjmp
	nop

ALTENTRY(longjmp)
	/* XXX have to dup this from below */
	sub,>>	%sp, %arg0, %r0		/* botch if jbuf was on stack */
	bl,n	longjmperror, %rp
	nop
	ldw	32(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

	std	%rp, HPPA_FRAME_RP(%sp)
	std	%arg0, HPPA_FRAME_ARG(0)(%sp)
	std	%arg1, HPPA_FRAME_ARG(1)(%sp)

	copy	%r3, %r1
	copy	%sp, %r3
	std,ma	%r1, HPPA_FRAME_SIZE(%sp)

	bl	sigsetmask, %rp
	ldd	8(%arg0), %arg0

	ldo	HPPA_FRAME_SIZE(%r3), %sp
	ldd,mb	-HPPA_FRAME_SIZE(%sp),%r3
	ldd	HPPA_FRAME_RP(%sp), %rp
	ldd	HPPA_FRAME_ARG(0)(%sp), %arg0
	b	_longjmp$restores
	ldd	HPPA_FRAME_ARG(1)(%sp), %arg1

ALTENTRY(_longjmp)
	sub,>>	%sp, %arg0, %r0		/* botch if jbuf was on stack */
	bl,n	longjmperror, %rp
	nop
	ldd	32(%arg0), %r1		/* botch if it's a real signal frame */
	add,=	%r0, %r1, %r0
	bl,n	longjmperror, %rp
	nop

_longjmp$restores
	/* restore callee-saved registers */
	ldo	96(%arg0), %r1
	ldd,ma	8(%r1), %r3
	ldd,ma	8(%r1), %r4
	ldd,ma	8(%r1), %r5
	ldd,ma	8(%r1), %r6
	ldd,ma	8(%r1), %r7
	ldd,ma	8(%r1), %r8
	ldd,ma	8(%r1), %r9
	ldd,ma	8(%r1), %r10
	ldd,ma	8(%r1), %r11
	ldd,ma	8(%r1), %r12
	ldd,ma	8(%r1), %r13
	ldd,ma	8(%r1), %r14
	ldd,ma	8(%r1), %r15
	ldd,ma	8(%r1), %r16
	ldd,ma	8(%r1), %r17
	ldd,ma	8(%r1), %r18

	/* restore the rest */
	ldd	24(%arg0), %sp
	ldd	40(%arg0), %rp		/* check the priv level */
	sub,<>	%arg1, %r0, %r0
	ldo	1(%arg1), %arg1
	bv	%r0(%rp)
	copy	%arg1, %ret0
EXIT(_longjmp)

	.end
