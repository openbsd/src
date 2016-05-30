/* $OpenBSD: sigsetjmp.S,v 1.10 2016/05/30 02:11:21 guenther Exp $ */
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

	.global	__jmpxor

ENTRY(sigsetjmp)
	movl	4(%esp),%ecx
	movl	8(%esp),%eax
	movl	%eax,28(%ecx)
	testl	%eax,%eax
	jz	1f

	pushl	$0			/* mask = empty */
	pushl	$1			/* how = SIG_BLOCK */
	subl	$4,%esp
	movl	$(SYS_sigprocmask),%eax
	int	$0x80			/* leave oset in %eax */
	addl	$12,%esp
	movl	%eax,24(%ecx)

1:	call	2f
2:	popl	%edx
	addl	$__jmpxor-2b,%edx	# load cookie address

	movl	%ebx, 4(%ecx)
	movl	%esp,   %eax
	xorl	8(%edx),%eax		# use esp cookie
	movl	%eax, 8(%ecx)
	movl	%ebp,   %eax
	xorl	0(%edx),%eax		# use ebp cookie
	movl	%eax,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	4(%edx),%edx		# load eip cookie over cookie address
	xorl	0(%esp),%edx
	movl	%edx, 0(%ecx)
	xorl	%eax,%eax
	ret
END(sigsetjmp)

ENTRY(siglongjmp)
	movl	4(%esp),%edx
	cmpl	$0,28(%edx)
	jz	1f

	pushl	24(%edx)		/* mask from sc_mask */
	pushl	$3			/* how = SIG_SETMASK */
	subl	$4,%esp
	movl	$(SYS_sigprocmask),%eax
	int	$0x80
	addl	$12,%esp

1:	call	2f
2:	popl	%ecx
	addl	$__jmpxor-2b,%ecx	# load cookie address

	movl	 4(%esp),%edx		# reload in case sigprocmask failed
	movl	 8(%esp),%eax
	movl	 4(%edx),%ebx
	movl	 8(%edx),%esi		# load xor'ed esp into safe register
	xorl	 8(%ecx),%esi		# use esp cookie
	movl	   %esi, %esp		# un-xor'ed esp is safe to use
	movl	12(%edx),%ebp
	xorl	 0(%ecx),%ebp		# use ebp cookie
	movl	16(%edx),%esi
	movl	20(%edx),%edi

	movl	 4(%ecx),%ecx		# load eip cookie over cookie address
	xorl	 0(%edx),%ecx
	testl	%eax,%eax
	jnz	2f
	incl	%eax
2:	movl	%ecx,0(%esp)
	ret
END(siglongjmp)
