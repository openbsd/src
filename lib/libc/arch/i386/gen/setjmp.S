/* $OpenBSD: setjmp.S,v 1.11 2016/05/30 02:11:21 guenther Exp $ */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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

#include "SYS.h"

	.section	.openbsd.randomdata,"aw",@progbits
	.balign	4
	.globl	__jmpxor
	.hidden	__jmpxor
__jmpxor:
	.zero	4*3		# (eip, esp, ebp)
	END(__jmpxor)
	.type	__jmpxor,@object


/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(setjmp)
	pushl	$0			/* mask = empty */
	pushl	$1			/* how = SIG_BLOCK */
	call	1f
1:	movl	$(SYS_sigprocmask),%eax
	int	$0x80			/* leave oset in %eax */
	popl	%edx
	addl	$8,%esp
	addl	$__jmpxor-1b,%edx	# load cookie address

	movl	4(%esp),%ecx
	movl	%eax,24(%ecx)
	movl	%ebx, 4(%ecx)
	movl	%esp,   %eax
	xorl	0(%edx),%eax		# use esp cookie
	movl	%eax, 8(%ecx)
	movl	%ebp,   %eax
	xorl	4(%edx),%eax		# use ebp cookie
	movl	%eax,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	8(%edx),%edx		# load eip cookie over cookie address
	xorl	0(%esp),%edx
	movl	%edx, 0(%ecx)
	xorl	%eax,%eax
	ret
END(setjmp)

ENTRY(longjmp)
	movl	4(%esp),%edx
	pushl	24(%edx)		/* mask from sc_mask */
	pushl	$3			/* how = SIG_SETMASK */
	call	1f			/* get our eip */
1:	movl	$(SYS_sigprocmask),%eax
	int	$0x80
	popl	%ecx
	addl	$8,%esp
	addl	$__jmpxor-1b,%ecx	# load cookie address

	movl	 4(%esp),%edx
	movl	 8(%esp),%eax
	movl	 4(%edx),%ebx
	movl	 8(%edx),%esi		# load xor'ed esp into safe register
	xorl	 0(%ecx),%esi		# use esp cookie
	movl	   %esi, %esp		# un-xor'ed esp is safe to use
	movl	12(%edx),%ebp
	xorl	 4(%ecx),%ebp		# use ebp cookie
	movl	16(%edx),%esi
	movl	20(%edx),%edi

	movl	8(%ecx),%ecx		# load eip cookie over cookie address
	xorl	0(%edx),%ecx
	testl	%eax,%eax
	jnz	1f
	incl	%eax
1:	movl	%ecx,0(%esp)
	ret
END(longjmp)
