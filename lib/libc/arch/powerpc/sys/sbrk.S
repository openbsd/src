/* $OpenBSD: sbrk.S,v 1.6 2002/10/07 04:47:12 drahn Exp $ */

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
	.globl _ASM_LABEL(__curbrk)
	.globl minbrk
_ASM_LABEL(__curbrk):
	.long _C_LABEL(end)
_ASM_LABEL(minbrk):
	.long _C_LABEL(end)


	.text
PREFIX2(sbrk,break)
	
	/* call break(__curbrk + size) */
#ifndef PIC
	addis	6, 0, _ASM_LABEL(__curbrk)@H
	ori	6, 6, _ASM_LABEL(__curbrk)@L	/* # 6 = &__curbrk */
#else
	mflr	10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	9
	mtlr	10
	lwz	6,_ASM_LABEL(__curbrk)@got(9)
#endif
	lwz	5, 0(6)		/* # 5 = *6 (old_curbrk) */
	add	3, 5, 3		/* # 3 = new_curbrk */
	mr	7, 3

	sc

	/* check for error */
	cmpwi	0, 0
	beq+	.L_sbrk_ok
	b	PIC_PLT(_ASM_LABEL(__cerror))

	/* update, __curbrk and return */
.L_sbrk_ok:
	stw	7, 0(6)		/* # remember, 6=&__curbrk, 7=new_curbrk */
	mr	3, 5		/* # remember, 5=old_curbrk */
	blr
