/*	$OpenBSD: srt0.S,v 1.3 2010/06/29 21:33:54 miod Exp $	*/
/*	$NetBSD: srt0.S,v 1.5.4.2 1996/07/17 01:51:46 jtc Exp $	*/

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
#include <machine/asm.h>

#define	CCFSZ	96

	.file	"str0.s"

	.data
	.global	_C_LABEL(cputyp), _C_LABEL(nbpg), _C_LABEL(pgofset)
	.global _C_LABEL(pgshift)
_C_LABEL(cputyp):
	.word	1
_C_LABEL(nbpg):
	.word	1
_C_LABEL(pgofset):
	.word	1
_C_LABEL(pgshift):
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
	 * Find which address we are at.
	 */
1:	call	2f
	 sethi	%hi(1b), %l0
2:	or	%l0, %lo(1b), %l0
	cmp	%l0, %o7		! %o7 contains actual address of 1b
	beq	4f			! already there, no need to relocate
	 nop

	/*
	 * Relocate.
	 */
	add	%o7, (start-1b), %l0
	set	start, %l1
	set	_C_LABEL(end), %o0
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
	set     _C_LABEL(edata), %o0	! bzero(edata, end - edata)
	set     _C_LABEL(end), %o1
	/* note: bzero() expanded inline for compactness */
	subcc	%o1, %o0, %o1
	bz	2f			! in case there is no BSS

	srl	%o1, 2, %o1		! assume edata and end are aligned
1:
	st	%g0, [%o0]		! while (n--)
	subcc	%o1, 1, %o1		!	*p++ = 0;
	bnz	1b
	 add	%o0, 4, %o0
2:

	/*
	 * Enable interrupts, but only above level 11. This enables "L1-A",
	 * but avoids spurious interrupt bites from most other devices.
	 */
	rd	%psr, %o0
	andn	%o0, PSR_PIL, %o0
	wr	%o0, 0xb00, %psr	! (11 << 8)
	nop; nop; nop

	/*
	 * Set CPU type that we are running on.
	 */
	sethi	%hi(_C_LABEL(cputyp)), %o0
	set	0x4000, %g7
	cmp	%i0, %g7
	beq	5f
	 nop

	/*
	 * Save address of PROM vector (passed in %i0).
	 */
	sethi	%hi(_C_LABEL(promvec)), %o1
	st	%i0, [%o1 + %lo(_C_LABEL(promvec))]

	mov	CPU_SUN4C, %g4
	mov	SUN4CM_PGSHIFT, %g5
	b,a	6f

5:
	mov	CPU_SUN4, %g4
	mov	SUN4_PGSHIFT, %g5

6:
	st	%g4, [%o0 + %lo(_C_LABEL(cputyp))]
	sethi	%hi(_C_LABEL(pgshift)), %o0	! pgshift = log2(nbpg)
	st	%g5, [%o0 + %lo(_C_LABEL(pgshift))]

	mov	1, %o0				! nbpg = 1 << pgshift
	sll	%o0, %g5, %g5
	sethi	%hi(_C_LABEL(nbpg)), %o0	! nbpg = bytes in a page
	st	%g5, [%o0 + %lo(_C_LABEL(nbpg))]

	sub	%g5, 1, %g5
	sethi	%hi(_C_LABEL(pgofset)), %o0	! page offset = bytes in a page - 1
	st	%g5, [%o0 + %lo(_C_LABEL(pgofset))]

	call	_C_LABEL(main)
	 mov	%i0, %o0

	ret
	 restore
