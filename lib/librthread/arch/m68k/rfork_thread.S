/*	$OpenBSD: rfork_thread.S,v 1.1 2005/12/23 18:50:23 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../../../libc/arch/m68k/SYS.h"

/*
 * int rfork_thread(int flags, void *stack, void (*func)(void *), void *arg);
 */
ENTRY(rfork_thread)
	/*
	 * Set up the new thread's stack.
	 */
	movl	sp@(8), a0	/* stack */
	movl	sp@(16), a0@-	/* arg */
	movl	sp@(12), a1	/* func */

	/*
	 * We did not create a frame, so the stack is ready for an immediate
	 * system call invocation.
	 */
	__DO_SYSCALL(rfork)
	jcs	9f

	tstl	d0
	jeq	1f

	/*
	 * parent process: just return.
	 */
	rts

1:
	/*
	 * child process: switch stacks, invoke function, then exit.
	 */
	movl	a0, sp		/* stack with arg pushed on it */
	jsr	a1@		/* func */
	addq	#4, sp

	__DO_SYSCALL(threxit)

9:
	/*
	 * system call failure.
	 */
	jra	__cerror
