/*	$OpenBSD: rfork_thread.S,v 1.1 2005/12/28 19:11:25 miod Exp $	*/

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

#include "../../../libc/arch/hppa/SYS.h"

/*
 * int rfork_thread(int flags, void *stack, void (*func)(void *), void *arg);
 */
ENTRY(rfork_thread, 0)
	SYSCALL(rfork)
	comb,<>	r0, ret0, 1f
	 nop

	/*
	 * In child process: switch stack, invoke function, then exit.
	 */

	/*
	 * PIC code expects 32 bytes of room available below sp.
	 * Then the regular procedure invocation requires us to allocate
	 * 64 bytes as well.
	 */
	copy	arg1, sp
	ldo	0(sp), r3
	stw,ma	r0, HPPA_FRAME_SIZE(sp)
	stw	r0, HPPA_FRAME_CRP(sp)
	stw	r0, HPPA_FRAME_PSP(sp)

	copy	arg3, arg0		/* arg */
	copy	arg2, t1
	bl	$$dyncall, r31
	copy	r31, rp

	copy	r0, arg0
	SYSCALL(threxit)

1:
	bv	r0(rp)
	 nop

EXIT(rfork_thread)
