/*	$OpenBSD: rfork_thread.S,v 1.1 2005/12/24 12:01:26 miod Exp $	*/

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

#include "../../../libc/arch/mips64/SYS.h"

/*
 * int rfork_thread(int flags, void *stack, void (*func)(void *), void *arg);
 */
LEAF(rfork_thread, 32)
	/* a0 = flags, a1 = stack, a2 = func, a3 = arg */
	PTR_SUBU sp, 32
	SETUP_GP64(16, _C_LABEL(rfork_thread))

	move	t0, a1		/* stack */
	move	t1, a3		/* arg */

	__DO_SYSCALL(rfork)
	bnez	a3, 9f

	RESTORE_GP64
	PTR_ADDU sp, 32

	beqz	v0, 1f

	/*
	 * In parent process: just return.
	 */
	j	ra
	 nop

1:
	/*
	 * In child process: switch stack, invoke function, then exit.
	 */

	move	sp, t0		/* stack */
	move	t9, a2		/* func */
	move	a0, t1		/* arg */
	jal	ra, t9
	 move	v0, zero

	move	a0, zero
	__DO_SYSCALL(threxit)

9:
	/*
	 * System call failure.
	 */
	LA	t9, _C_LABEL(cerror)

	RESTORE_GP64
	PTR_ADDU sp, 32
	jr	t9
	 nop
END(rfork_thread)
