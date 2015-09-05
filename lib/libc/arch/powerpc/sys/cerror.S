/* $OpenBSD: cerror.S,v 1.11 2015/09/05 06:22:47 guenther Exp $ */

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
	cmpwi	2, 0
	bne-	.L_threaded

#if defined (__PIC__)
	mflr	3			/* error value is in r0, use r3 */
	bcl	20, 31, 1f
1:	mflr	4
	addis	4, 4, _GLOBAL_OFFSET_TABLE_-1b@ha
	addi	4, 4, _GLOBAL_OFFSET_TABLE_-1b@l
	lwz	5,_C_LABEL(errno)@got(4)
	mtlr	3
#else
	addis 5, 0, errno@H
	ori 5, 5, errno@L
#endif
	b	.L_do_store

.L_threaded:
	lwz  5, 4(2)
	lwz  5, ERRNOPTR_OFFSET(5)

.L_do_store:
	stw 0, 0(5)
	addi 3, 0, -1
	addi 4, 0, -1
	blr
END(___cerror)
