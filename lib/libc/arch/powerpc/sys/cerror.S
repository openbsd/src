/* $OpenBSD: cerror.S,v 1.12 2015/09/09 08:24:12 kettenis Exp $ */

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
#include <machine/asm.h>

#define ERRNOPTR_OFFSET	16

WEAK_ALIAS(__cerror, ___cerror)

	.text
	.protected _ASM_LABEL(___cerror)
ASENTRY(___cerror)
	stwu	1, -16(1)		/* follow the SVR4 ABI */
	mflr	3			/* error value is in r0, use r3 */
	stw	3, 20(1)
	stw	31, 12(1)
#ifdef __PIC__
	stw	30, 8(1)
	bcl	20, 31, 1f
1:	mflr	30
	addis	30, 30, _GLOBAL_OFFSET_TABLE_-1b@ha
	addi	30, 30, _GLOBAL_OFFSET_TABLE_-1b@l
#endif
	mr	31, 0
	bl	PIC_PLT(__errno)
	stw	31, 0(3)
#ifdef __PIC__
	lwz	30, 8(1)
#endif
	lwz	31, 12(1)
	lwz	0, 20(1)
	mtlr	0
	addi	1, 1, 16	/* fllow the SVR4 ABI */
	li	3, -1
	li	4, -1
	blr
END(___cerror)
