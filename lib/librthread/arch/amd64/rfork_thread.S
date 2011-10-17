/*	$OpenBSD: rfork_thread.S,v 1.5 2011/10/17 06:39:20 guenther Exp $ */
/*-
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
 * Copyright (c) 2003 Alan L. Cox <alc@cs.rice.edu>
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
__FBSDID("$FreeBSD: /repoman/r/ncvs/src/lib/libc/amd64/gen/rfork_thread.S,v 1.1 2003/10/13 20:32:33 alc Exp $");
#endif

/*
 * With thanks to John Dyson for the original version of this.
 */

#include "../../../libc/arch/amd64/SYS.h"

/*
 *                %rdi   %rsi        %rdx       %rcx
 * __tfork_thread(param, stack_addr, start_fnc, start_arg);
 *
 * param:		Argument to pass to the actual kernel call.
 * stack_addr:		Top of stack for thread.
 * start_fnc:		Address of thread function to call in child.
 * start_arg:		Argument to pass to the thread function in child.
 */

ENTRY(__tfork_thread)
	movq	%rdx, %r8
	movq	%rcx, %r9

	/*
	 * Prepare and execute the thread creation syscall
	 */
	movl	$SYS___tfork, %eax
	syscall
	jb 	2f

	/*
	 * Check to see if we are in the parent or child
	 */
	cmpl	$0, %edx
	jnz	1f
	ret

	/*
	 * If we are in the child (new thread), then
	 * set-up the call to the internal subroutine.  If it
	 * returns, then call threxit.
	 */
1:
	movq	%rsi, %rsp
	movq	%r9, %rdi
	call	*%r8

	/*
	 * Thread exit system call
	 */
	movl	$SYS_threxit, %eax
	xorl	%edi, %edi
	syscall

	/*
	 * Branch here if the thread creation fails:
	 */
2:
#ifdef PIC
	movq	PIC_GOT(CERROR), %rcx
	jmp	*%rcx
#else
	jmp	CERROR
#endif
