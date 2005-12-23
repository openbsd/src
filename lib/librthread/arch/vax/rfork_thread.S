/*	$OpenBSD: rfork_thread.S,v 1.1 2005/12/23 19:52:04 miod Exp $	*/

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

#include "../../../libc/arch/vax/SYS.h"

/*
 * int rfork_thread(int flags, void *stack, void (*func)(void *), void *arg);
 */
ENTRY(rfork_thread, R2|R3|R4)
	/*
	 * Save thread creation arguments into registers.
	 */
	movl	8(ap), r2	/* stack */
	movl	12(ap), r3	/* func */
	movl	16(ap), r4	/* arg */

	__DO_SYSCALL(rfork)
	jcs	9f

	cmpl	r0, $0
	beql	1f

	/*
	 * In parent process: just return.
	 */
	ret

1:
	/*
	 * In child process: switch stack, invoke function, then exit.
	 * Note that since we can not pass a register to calls, we need
	 * to waste 4 bytes of stack in every thread.
	 */
	movl	r2, sp		/* stack */
	pushl	r3		/* func */
	pushl	r4		/* arg */
	calls	$1, *4(sp)	/* func */

	__DO_SYSCALL(threxit)

9:
	/*
	 * system call failed.
	 */
	jmp	__cerror
