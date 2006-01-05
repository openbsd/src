/*	$OpenBSD: sbrk.S,v 1.3 2006/01/05 22:19:49 kettenis Exp $	*/

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

	.import	end, data

	.data
	.export	__curbrk, data
__curbrk
	.long	end

SYSENTRY(sbrk)
#ifdef PIC
	addil	LT%__curbrk, r19
	ldd	RT%__curbrk(r1), t1
	ldd	0(t1), t2
#else
	ldil	L%__curbrk, t1
	ldd	R%__curbrk(t1), t2
#endif
	add	arg0, t2, arg0
	std	arg0, HPPA_FRAME_ARG(0)(sp)

	SYSCALL(break)

#ifdef PIC
	addil	LT%__curbrk, r19
	ldd	RT%__curbrk(r1), t1
#else
	ldil	L%__curbrk, t1
	ldo	R%__curbrk(t1), t1
#endif
	ldd	HPPA_FRAME_ARG(0)(sp), arg0
	ldd	0(t1), ret0
	bv	r0(rp)
	std	arg0, 0(t1)

SYSEXIT(sbrk)

	.end

