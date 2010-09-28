/*	$OpenBSD: mutex.S,v 1.7 2010/09/28 20:27:55 miod Exp $	*/

/*
 * Copyright (c) 2007 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Mutex implementation based on Example 9 from Appendix J of
 * SPARC-V9/R1.4.5, The SPARC Architecture Manual.
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/param.h>

#ifdef MULTIPROCESSOR

#define GET_CURCPU(ci) \
	ldx	[%g7 + CI_SELF], ci

#else

#define GET_CURCPU(ci) \
	set	CPUINFO_VA, ci

#endif


ENTRY(mtx_init)
	stx	%g0, [%o0 + MTX_OWNER]
	stw	%o1, [%o0 + MTX_WANTIPL]
	retl
	 stw	%g0, [%o0 + MTX_OLDIPL]

ENTRY(mtx_enter)
	rdpr	%pil, %g4
	GET_CURCPU(%g1)
1:
	ld	[%o0 + MTX_WANTIPL], %g5
	cmp	%g4, %g5
	bge	2f
	 nop
	wrpr	%g5, %pil
2:
	mov	%g1, %g5
/*
 * The assembler doesn't like the next line, even if MTX_OWNER is 0.
 */
!	casx	[%o0 + MTX_OWNER], %g0, %g5
	casx	[%o0], %g0, %g5
	tst	%g5
	be	4f
	 nop
	wrpr	%g4, %pil
3:
	ldx	[%o0 + MTX_OWNER], %g5
	tst	%g5
	bne	3b
	 nop
	ba,a	1b
4:
	stw	%g4, [%o0 + MTX_OLDIPL]
#ifdef DIAGNOSTIC
	ld	[%g1 + CI_MUTEX_LEVEL], %g5
	add	%g5, 1, %g5
	st	%g5, [%g1 + CI_MUTEX_LEVEL]
#endif
	retl
	 membar	#LoadLoad | #LoadStore

ENTRY(mtx_enter_try)
	rdpr	%pil, %g4
	GET_CURCPU(%g1)
1:
	ld	[%o0 + MTX_WANTIPL], %g5
	bge	2f
	 nop
	wrpr	%g5, %pil
2:
	mov	%g1, %g5
/*
 * The assembler doesn't like the next line, even if MTX_OWNER is 0.
 */
!	casx	[%o0 + MTX_OWNER], %g0, %g5
	casx	[%o0], %g0, %g5
	tst	%g5
	be	3f
	 nop
	wrpr	%g4, %pil
	retl
	 mov	0, %o0
3:
	stw	%g4, [%o0 + MTX_OLDIPL]
#ifdef DIAGNOSTIC
	ld	[%g1 + CI_MUTEX_LEVEL], %g5
	add	%g5, 1, %g5
	st	%g5, [%g1 + CI_MUTEX_LEVEL]
#endif
	membar	#LoadLoad | #LoadStore
	retl
	 mov	1, %o0

ENTRY(mtx_leave)
#ifdef DIAGNOSTIC
	GET_CURCPU(%g1)
	ld	[%g1 + CI_MUTEX_LEVEL], %g5
	sub	%g5, 1, %g5
	st	%g5, [%g1 + CI_MUTEX_LEVEL]
#endif
	ld	[%o0 + MTX_OLDIPL], %g1
	membar	#StoreStore | #LoadStore
	stx	%g0, [%o0 + MTX_OWNER]
	retl
	 wrpr	%g1, %pil
