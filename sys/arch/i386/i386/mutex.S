/*	$OpenBSD: mutex.S,v 1.4 2007/05/27 18:34:01 art Exp $	*/

/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"
/*
 * Yeah, we don't really need to implement mtx_init here, but let's keep
 * all the functions in the same place.
 */
ENTRY(mtx_init)
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	movl	%edx, MTX_WANTIPL(%eax)
	xorl	%edx, %edx
	movl	%edx, MTX_OLDIPL(%eax)
	movl	%edx, MTX_LOCK(%eax)
	movl	%edx, MTX_OWNER(%eax)
	leave
	ret

#define SOFF	8

ENTRY(mtx_enter)
	pushl	%ebp
	movl	%esp, %ebp	
1:	movl	SOFF(%ebp), %ecx
	movl	MTX_WANTIPL(%ecx), %eax
	movl	CPL, %edx		# oipl = cpl;
	cmpl	%edx, %eax		# if (cpl < mtx->mtx_wantipl)
	jle	2f
	movl	%eax, CPL		#	cpl = mtx->mtx_wantipl;
2:	/*
	 * %edx now contains the oldipl.
	 * %ecx contains the mtx.
	 */
	movl	$1, %eax
	xchgl	%eax, MTX_LOCK(%ecx)	# test_and_set(mtx->mtx_lock)
	testl	%eax, %eax		# if (already held)
	jnz	3f
	movl	CPUVAR(SELF), %eax
	movl	%eax, MTX_OWNER(%ecx)
	movl	%edx, MTX_OLDIPL(%ecx)
	leave
	ret

	/* We failed to obtain the lock. splx, spin and retry. */
3:	pushl	%edx
	call	_C_LABEL(splx)
	movl	%ebp, %esp
	movl	SOFF(%ebp), %ecx		# %ecx clobbered
4:
#ifdef DIAGNOSTIC
	movl	CPUVAR(SELF), %edx
	cmpl	MTX_OWNER(%ecx), %edx
	je	5f
#endif
	movl	MTX_LOCK(%ecx), %eax
	testl	%eax, %eax
	jz	1b
	jmp	4b
#ifdef DIAGNOSTIC
5:	pushl	$6f
	call	_C_LABEL(panic)
6:	.asciz	"mtx_enter: locking against myself"
#endif
	
ENTRY(mtx_leave)
	pushl	%ebp
	movl	%esp, %ebp
	movl	SOFF(%ebp), %ecx
	xorl	%eax, %eax
	movl	%eax, MTX_OWNER(%ecx)
	pushl	MTX_OLDIPL(%ecx)
	movl	%eax, MTX_OLDIPL(%ecx)
	movl	%eax, MTX_LOCK(%ecx)
	call	_C_LABEL(splx)
	leave
	ret
