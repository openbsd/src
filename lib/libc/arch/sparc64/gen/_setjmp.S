/*	$NetBSD: _setjmp.S,v 1.4 1998/10/08 02:27:59 eeh Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v?v:1)" from
 * the last call to
 *	_setjmp(a)
 * by unwinding the call stack.
 * The previous signal state is NOT restored.
 */

ENTRY(_setjmp)
	stx	%sp, [%o0+0x00]	/* store caller's stack pointer */
	stx	%o7, [%o0+0x08]	/* ... return pc */
	stx	%fp, [%o0+0x10]	/* ... and frame pointer */
	retl
	 clr	%o0		! return 0

ENTRY(_longjmp)
	mov	1, %g6
	movrnz	%o1, %o1, %g6	! compute v ? v : 1 in a global register
	mov	%o0, %g1	! save a in another global register
	ldx	[%g1+0x10], %g7	/* get caller's frame */
1:
	cmp	%fp, %g7	! compare against desired frame
	bl,a	1b		! if below,
	 restore		!    pop frame and loop
	be,a	2f		! if there,
	 ldx	[%g1+0], %o2	!    fetch return %sp

Lbotch:
	call	_C_LABEL(longjmperror)	! otherwise, went too far; bomb out
	nop
	unimp	0

2:
	cmp	%o2, %sp	! %sp must not decrease
	bge,a	3f
	 mov	%o2, %sp	! it is OK, put it in place
	b,a	Lbotch
3:
	ldx	[%g1+0x8], %o3	! fetch pc
	jmp	%o3 + 8		! success, return %g6
	 mov	%g6, %o0

