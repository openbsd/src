/*	$OpenBSD: brk.S,v 1.8 2002/10/07 09:35:34 mickey Exp $	*/

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
	.text
	.asciz "$OpenBSD: brk.S,v 1.8 2002/10/07 09:35:34 mickey Exp $"
	.align	4
#endif

	.import	__curbrk, data
	.import	end, data

	.data
	.export	__minbrk, data
__minbrk
	.long	end

ENTRY(brk,0)
	ldil	L%__minbrk, t1
	ldw	R%__minbrk(t1), t2
	sub,<<	t2, arg0, r0
	copy	t2, arg0
	stw	arg0, HPPA_FRAME_ARG(0)(sp)

	SYSCALL(break)

	ldw	HPPA_FRAME_ARG(0)(sp), arg0
	ldil	L%__curbrk, t1
	bv	r0(rp)
	stw	arg0, R%__curbrk(t1)
EXIT(brk)

	.end

