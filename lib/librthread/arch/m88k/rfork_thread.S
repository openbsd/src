/*	$OpenBSD: rfork_thread.S,v 1.3 2012/01/17 02:34:18 guenther Exp $	*/

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

#include "../../../libc/arch/m88k/SYS.h"

/*
 * int __tfork_thread(const struct __tfork *param, void *stack, void (*func)(void *), void *arg);
 */
ENTRY(__tfork_thread)
	or	r6,  r3,  r0	/* save stack */
	or	r13, r0,  __SYSCALLNAME(SYS_,__tfork)
	tb0	0,   r0,  128	/* corrupts r2 and r3 in the child */
	br	__cerror

	bcnd	eq0, r2,  1f

	/*
	 * In parent process: just return.
	 */
	jmp	r1

1:
	/*
	 * In child process: switch stack, invoke function, then exit.
	 */
	or	r31, r6,  r0	/* stack */
	jsr.n	r4		/* func */
	 or	r2,  r5,  r0	/* arg */

	or	r13, r0,  __SYSCALLNAME(SYS_,__threxit)
	tb0	0,   r0,  128
