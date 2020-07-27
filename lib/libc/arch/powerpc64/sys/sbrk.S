/* $OpenBSD: sbrk.S,v 1.3 2020/07/27 05:08:57 gkoehler Exp $ */

/*
 * Copyright (c) 1996 Dale Rahn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "SYS.h"

	.data
	.globl _end
	.globl __curbrk
	.hidden __curbrk
__curbrk:
	.quad	_end
	END(__curbrk)
	.type	__curbrk,@object

	.weak	sbrk

	.text
PSEUDO_PREFIX(,sbrk,break)
	/* call break(__curbrk + size) */
	addis	%r6, %r2, __curbrk@toc@ha
	addi	%r6, %r6, __curbrk@toc@l	/* # %r6 = &__curbrk */

	ld	%r5, 0(%r6)			/* # %r5 = *%r6 (old_curbrk) */
	add	%r3, %r5, %r3			/* # %r3 = new_curbrk */
	mr	%r7, %r3

	sc

	/* check for error */
	cmpwi	%r0, 0
	beq+	.L_sbrk_ok
	stw	%r0, R13_OFFSET_ERRNO(%r13)
	li	%r3, -1
	blr

	/* update, __curbrk and return */
.L_sbrk_ok:
	std	%r7, 0(%r6)	/* # remember, %r6=&__curbrk, %r7=new_curbrk */
	mr	%r3, %r5	/* # remember, %r5=old_curbrk */
	blr
END(sbrk)
