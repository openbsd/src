/*	$OpenBSD: bzero.S,v 1.2 2001/08/20 20:15:29 jason Exp $	*/
/*	$NetBSD: bzero.S,v 1.1.1.1 1998/06/20 05:18:14 eeh Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 * Header: bzero.s,v 1.1 92/06/25 12:52:46 torek Exp
 */

#if defined(LIBC_SCCS) && !defined(lint)
#ifdef notdef
	.asciz "@(#)bzero.s	8.1 (Berkeley) 6/4/93"
#endif
	.asciz "$NetBSD: bzero.S,v 1.1.1.1 1998/06/20 05:18:14 eeh Exp $"
#endif  /* LIBC_SCCS and not lint */

#include "DEFS.h"

/*
 * bzero(addr, len)
 *
 * We should unroll the loop, but at the moment this would
 * gain nothing since the `std' instructions are what limits us.
 *
 * I just changed all the `std's to `stx's.  It still needs proper
 * optimization using the block load/store ASIs.
 */
ENTRY(bzero)
#ifdef NOT_DEBUG
	save	%sp, -176, %sp
	mov	%i0, %o1
	set	9f, %o0
	call	prom_printf
	 mov	%i1, %o2
	ba	8f
	 restore
9:	.asciz	"bzero(%p,%x)\r\n"
	.align	4
8:
#endif
	! %o0 = addr, %o1 = len

	! Optimize a common case: addr and len are both multiples of 8.
	or	%o0, %o1, %o2
	andcc	%o2, 7, %g0		! ((addr | len) & 7) != 0?
	bnz	1f			! if so, cannot optimize
	 clr	%g1			! in any case, we want g1=0

	/* `Good' operands, can just store doubles. */
0:
	deccc	8, %o1			! while ((len -= 8) >= 0)
	bge,a	0b
	 stx	%g0, [%o0 + %o1]	!	*(quad *)(addr + len) = 0;
	retl
	 nop

	/*
	 * Either the address is unaligned, or the count is not a
	 * multiple of 8, or both.  We will have to align the address
	 * in order to use anything `better' than stb.
	 */
1:
	cmp	%o1, 15			! len >= 15?
	bge,a	Lstd			! yes, use std
	 andcc	%o0, 1, %g0		! (but first check alignment)

	! not enough to bother: do byte-at-a-time loop.
2:
	deccc	%o1			! while (--len >= 0)
	bge,a	2b
	 stb	%g0, [%o0 + %o1]	!	addr[len] = 0;
	retl
	 nop

Lstd:
	/*
	 * There are at least 15 bytes to zero.
	 * We may have to zero some initial stuff to align
	 * the address.
	 */
	bz,a	1f			! if (addr & 1) {
	 andcc	%o0, 2, %g0
	stb	%g0, [%o0]		!	*addr = 0;
	inc	%o0			!	addr++;
	dec	%o1			!	len--;
	andcc	%o0, 2, %g0			! }
1:
	bz,a	1f			! if (addr & 2) {
	 andcc	%o0, 4, %g0
	sth	%g0, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
	dec	2, %o1			!	len -= 2;
	andcc	%o0, 4, %g0		! }
1:
	bz	1f			! if (addr & 4) {
	 dec	8, %o1
	st	%g0, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
	dec	4, %o1			!	len -= 4;
					! }
	/*
	 * Address is double word aligned; len is 8 less than
	 * the number of bytes remaining (i.e., len is 0 if
	 * the remaining count is 8, 1 if it is 9, etc.).
	 */
1:
	stx	%g0, [%o0]		! do {
2:					!	*(quad *)addr = 0;
	inc	8, %o0			!	addr += 8;
	deccc	8, %o1			! } while ((len -= 8) >= 0);
	bge,a	2b
	 stx	%g0, [%o0]

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
	andcc	%o1, 4, %g0
	bz	1f			! if (len & 4) {
	 andcc	%o1, 2, %g0
	st	%g0, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
1:
	bz	1f			! if (len & 2) {
	 andcc	%o1, 1, %g0
	sth	%g0, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
1:
	bnz,a	1f			! if (len & 1)
	 stb	%g0, [%o0]		!	*addr = 0;
1:
	retl
	 nop
