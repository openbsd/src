/*	$OpenBSD: sigprocmask.S,v 1.6 2000/01/25 01:54:22 mickey Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SYS.h"

#ifdef SYSLIBC_SCCS
	.space .text
	.subspa $code$
	.asciz "$OpenBSD: sigprocmask.S,v 1.6 2000/01/25 01:54:22 mickey Exp $"
	.align	4
#endif

ENTRY(sigprocmask)
	/* just in case compiler forgot about this */
	stw	arg2, HPPA_FRAME_ARG(2)(sp)

	comb,<>,n r0, arg1, sigprocmask$nblock

	b	sigprocmask$call
	ldi	1, arg0

sigprocmask$nblock
	ldw	0(arg1), arg1
	stw	arg1, HPPA_FRAME_ARG(1)(sp)

sigprocmask$call
	SYSCALL(sigprocmask)

	ldw	HPPA_FRAME_ARG(2)(sp), arg2
	add,=	r0, arg2, arg2
	stw	ret0, 0(arg2)
	bv	r0(rp)
	copy	r0, ret0
EXIT(sigprocmask)

	.end
