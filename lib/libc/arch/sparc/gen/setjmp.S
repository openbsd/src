/* $OpenBSD: setjmp.S,v 1.8 2016/05/23 08:57:20 guenther Exp $ */
/*
 *
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

/*
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v?v:1)" from
 * the last call to
 *	_setjmp(a)
 * by restoring the previous context.
 * The previous signal state is restored.
 */

#include "SYS.h"
#include <machine/trap.h>	/* T_FLUSHWIN */
#define	CCFSZ	96		/* from locore.S */

ENTRY(setjmp)
	mov	%o0, %o3		/* Save our jmp_buf in %o3 */

	mov	1, %o0			/* SIG_BLOCK */
	mov	SYS_sigprocmask, %g1	
	clr	%o1			/* sigprocmask(SIG_BLOCK, 0) */
	t	ST_SYSCALL
	st	%o0, [%o3 + 8]

	st	%sp, [%o0 + 0]		/* store caller's stack pointer */
	st	%o7, [%o0 + 4]		/* ... and return pc */
	retl
	 clr	%o0			/* return 0 */

ENTRY(longjmp)
	save	%sp, -CCFSZ, %sp
	t	ST_FLUSHWIN	! flush register windows out to the stack

	mov	3, %o0			/* SIG_SETMASK */
	ld	[%i0 + 8], %o1
	mov	SYS_sigprocmask, %g1	
	t	ST_SYSCALL

	ld	[%i0 + 0], %fp
	ld	[%i0 + 4], %i7

	tst	%i1	       ! compute v ? v : 1
	be,a	0f
	 mov	1, %i1
0:
	mov	%i1, %o0
	ret
	 restore
