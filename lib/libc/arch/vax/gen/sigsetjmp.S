/*
 * Copyright (c) 1983, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "$NetBSD: sigsetjmp.S,v 1.1 1995/12/09 13:26:46 ragge Exp $"
#endif /* LIBC_SCCS and not lint */

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *      siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *      sigsetjmp(a, savemask)
 * by restoring registers from the stack,
 * and dependent on savemask restores the
 * signal mask.
 */

#include "DEFS.h"
#include <machine/setjmp.h>

ENTRY(sigsetjmp, R6)
	movl	4(ap),r0		# get env pointer
	movl	8(ap),(_JBLEN*4)(r0)	# save "savemask"
	tstl	8(ap)			# do saving of signal mask?
	beql	L1
	jmp	_setjmp+2		# yep, do full setjmp
L1:	jmp	__setjmp+2		# nope, skip to _setjmp

ENTRY(siglongjmp, 0)
	movl	4(ap),r0		# get env pointer
	tstl	(_JBLEN*4)(r0)		# test if "savemask" was set
	beql	L2
	jmp	_longjmp+2		# yep, do full longjmp
L2:	jmp	__longjmp+2		# nope, skip to _longjmp


