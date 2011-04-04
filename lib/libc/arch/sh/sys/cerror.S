/*	$OpenBSD: cerror.S,v 1.2 2011/04/04 12:42:39 guenther Exp $	*/
/*	$NetBSD: cerror.S,v 1.10 2006/01/06 05:14:39 uwe Exp $	*/

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
 *	from: @(#)cerror.s	5.1 (Berkeley) 4/23/90
 */

#include "SYS.h"

WEAK_ALIAS(CERROR, _CERROR)

ASENTRY(_CERROR)
#ifdef _REENTRANT
	mov.l	.L___errno, r1
	PIC_PROLOGUE(.L_got)
	sts.l   pr, @-sp
1:	CALL	r1
	 mov.l	r4, @-sp	! save error code
	mov.l	@sp+, r4
	lds.l   @sp+, pr
	PIC_EPILOGUE
	mov.l	r4, @r0
	mov	#-1, r1
	rts
	 mov	#-1, r0

	.align	2
.L_got:		PIC_GOT_DATUM
.L___errno:	CALL_DATUM(_C_LABEL(__errno), 1b)
	SET_ASENTRY_SIZE(_CERROR)

#else /* !_REENTRANT */
#ifdef PIC
	mova	L_GOT, r0
	mov.l	L_GOT, r1
	add	r0, r1
	mov.l	L_errno, r0
	mov.l	@(r0, r1), r1
	mov.l	r4, @r1
#else
	mov.l	L_errno, r1
	mov.l	r4, @r1
#endif
	mov	#-1, r0
	rts
	 mov	#-1, r1

	.align	2
#ifdef PIC
L_GOT:		.long	_GLOBAL_OFFSET_TABLE_
L_errno:	.long	_C_LABEL(errno)@GOT
#else
L_errno:	.long	_C_LABEL(errno)
#endif
#endif /* !_REENTRANT */
