/*	$NetBSD: srt0.S,v 1.5 1995/12/13 23:35:18 pk Exp $	*/

/*
 * Copyright (c) 1994 Paul Kranenburg
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
 *	This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <machine/param.h>
#include <machine/psl.h>

#define	CCFSZ	96

	.file	"str0.s"

	.data
	.global	_cputyp, _nbpg, _pgofset, _pgshift
_cputyp:
	.word	1
_nbpg:
	.word	1
_pgofset:
	.word	1
_pgshift:
	.word	1

	.text	
	.globl	start

start:
	/*
	 * Set up a stack.
	 */
	set	start, %o1
	save	%o1, -CCFSZ, %sp

	/*
	 * Relocate.
	 */
1:	call	2f
	 nop
2:	add	%o7, (start-1b), %l0
	set	start, %l1
	set	_end, %o0
	sub	%o0, %l1, %l2		! length
3:	ld	[%l0], %o0
	add	%l0, 4, %l0
	st	%o0, [%l1]
	subcc	%l2, 4, %l2
	bg	3b
	 add	%l1, 4, %l1

	set	4f, %g1
	jmp	%g1
	 nop

4:
#ifdef notyet
	/*
	 * Enable traps
	 */
	wr	%g0, 0, %wim		! make sure we can set psr
	nop; nop; nop
	wr	%g0, PSR_S|PSR_PS|PSR_PIL, %psr	! set initial psr
	nop; nop; nop
	wr	%g0, 2, %wim		! set initial %wim (w1 invalid)

	rd	%psr, %l0
	wr	%l0, PSR_ET, %psr
	nop; nop; nop
#endif

	/*
	 * Clear BSS
	 */
	set     _edata, %o0             ! bzero(edata, end - edata)
	set     _end, %o1
	call    _bzero
	 sub    %o1, %o0, %o1

	/*
	 * Enable interrupts.
	 */
	rd	%psr, %o0
	andn	%o0, PSR_PIL, %o0
	wr	%g0, %o0, %psr
	nop; nop; nop

	/*
	 * Set CPU type that we are running on.
	 */
	sethi	%hi(_cputyp), %o0
	set	0x4000, %g7
	cmp	%i0, %g7
	beq	5f
	 nop

	/*
	 * Save address of PROM vector (passed in %i0).
	 */
	sethi	%hi(_promvec), %o1
	st	%i0, [%o1 + %lo(_promvec)]

	mov	CPU_SUN4C, %g4
	mov	SUN4CM_PGSHIFT, %g5
	b,a	6f

5:
	mov	CPU_SUN4, %g4
	mov	SUN4_PGSHIFT, %g5

6:
	st	%g4, [%o0 + %lo(_cputyp)]
	sethi	%hi(_pgshift), %o0	! pgshift = log2(nbpg)
	st	%g5, [%o0 + %lo(_pgshift)]

	mov	1, %o0			! nbpg = 1 << pgshift
	sll	%o0, %g5, %g5
	sethi	%hi(_nbpg), %o0		! nbpg = bytes in a page
	st	%g5, [%o0 + %lo(_nbpg)]

	sub	%g5, 1, %g5
	sethi	%hi(_pgofset), %o0	! page offset = bytes in a page - 1
	st	%g5, [%o0 + %lo(_pgofset)]

	call	_main
	 mov	%i0, %o0

	ret
	 restore


#ifdef TIGHT

/*
 * XXX - Space saving .div & .rem routines (small & non-negative numbres only)
 */
	.align	4
	.global .div, .udiv
! int n = 0; while (a >= b) { a -= b; n++; }; return n;
.div:
.udiv:
	cmp	%o0, %o1
	bl	2f
	 mov	0, %o5
1:
	sub	%o0, %o1, %o0
	cmp	%o0, %o1
	bge	1b
	 add	%o5, 1, %o5
2:
	retl
	 mov	%o5, %o0

	.align	4
	.global .rem, .urem
! while (a>=b) a -= b; return a;
.rem:
.urem:
	cmp	%o0, %o1
	bl	2f
	 nop
	sub	%o0, %o1, %o0
1:
	cmp	%o0, %o1
	bge,a	1b
	 sub	%o0, %o1, %o0
2:
	retl
	 nop

#endif /* TIGHT */
