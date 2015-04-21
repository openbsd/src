/*	$OpenBSD: tfork_thread.S,v 1.6 2015/04/21 01:24:28 guenther Exp $ */
/*-
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#if 0
__FBSDID("$FreeBSD: /repoman/r/ncvs/src/lib/libc/i386/gen/rfork_thread.S,v 1.5 2003/05/07 17:23:25 jhb Exp $");
#endif

/*
 * With thanks to John Dyson for the original version of this.
 */

#include "SYS.h"

/*
 *                8      12     16         20
 * __tfork_thread(param, psize, start_fnc, start_arg);
 *
 * param:		Arguments to actual system call.
 * psize:		Other argument to pass to the actual kernel call.
 * start_fnc:		Address of thread function to call in child.
 * start_arg:		Argument to pass to the thread function in child.
 */

ENTRY(__tfork_thread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi

	/*
	 * Save the thread info in esi and ebx
	 */
	movl	16(%ebp), %esi	# get start thread address
	movl	20(%ebp), %edi	# get start argument

	/*
	 * Prepare and execute the thread creation syscall
	 */
	pushl	12(%ebp)	# push psize
	pushl	8(%ebp)		# push param
	pushl	$0		# slot for return address, ignored by kernel
	movl	$SYS___tfork, %eax
	int	$0x80
	jb 	2f

	/*
	 * Check to see if we are in the parent or child
	 */
	cmpl	$0, %eax
	jz	1f
	addl	$12, %esp
	popl	%edi
	popl	%esi
	movl	%ebp, %esp
	popl	%ebp
	ret
	.p2align 2

	/*
	 * If we are in the child (new thread), then
	 * set-up the call to the internal subroutine.  If it
	 * returns, then call __threxit.
	 */
1:
	xorl	%ebp, %ebp	# mark outermost frame
	subl	$4, %esp	# align stack
	andl	$~15, %esp
	addl	$4, %esp
	pushl	%edi		# push start argument
	call	*%esi
	addl	$4, %esp

	/*
	 * Exit system call
	 */
	pushl	$0		# NULL pointer argument to __threxit
	pushl	$0		# slot for return address, ignored by kernel
	movl	$SYS___threxit, %eax
	int	$0x80

	/*
	 * Branch here if the thread creation fails:
	 */
2:
	addl	$12, %esp
	popl	%edi
	popl	%esi
	movl	%ebp, %esp
	popl	%ebp
#ifdef __PIC__
	PIC_PROLOGUE
	movl	PIC_GOT(CERROR), %ecx
	PIC_EPILOGUE
	jmp	*%ecx
#else
	jmp	CERROR
#endif
