/*	$OpenBSD: brk.S,v 1.1.1.1 2006/10/10 22:07:10 miod Exp $	*/
/*	$NetBSD: brk.S,v 1.10 2006/01/06 03:58:31 uwe Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)brk.s	5.2 (Berkeley) 12/17/90
 */

#include "SYS.h"

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk,_brk)
#endif

	.data
	.globl	_C_LABEL(__minbrk)
	.align	2
_C_LABEL(__minbrk):
	.long	_C_LABEL(end)

	.text

SYSENTRY(_brk)
#ifdef PIC
	mova	L_GOT, r0
	mov.l	L_GOT, r2
	add	r0, r2
	mov.l	Lminbrk, r0
	mov.l	@(r0, r2), r0
	mov.l	@r0, r0
#else
	mov.l	Lminbrk, r0
	mov.l	@r0, r0
#endif
	cmp/hs	r4, r0
	bf	1f
	mov	r0, r4
1:
	mov.l	LSYS_break, r0
	trapa	#0x80
	bf	err
#ifdef PIC
	mov.l	Lcurbrk, r0
	mov.l	@(r0, r2), r1
	xor	r0, r0
#else
	xor	r0, r0
	mov.l	Lcurbrk, r1
#endif
	rts
	mov.l	r4, @r1
err:
	JUMP_CERROR

	.align	2
LSYS_break:	.long	SYS_break
#ifdef PIC
L_GOT:		.long	_GLOBAL_OFFSET_TABLE_
Lminbrk:	.long	_C_LABEL(__minbrk)@GOT
Lcurbrk:	.long	_C_LABEL(__curbrk)@GOT
#else
Lminbrk:	.long	_C_LABEL(__minbrk)
Lcurbrk:	.long	_C_LABEL(__curbrk)
#endif
	SET_ENTRY_SIZE(_brk)
