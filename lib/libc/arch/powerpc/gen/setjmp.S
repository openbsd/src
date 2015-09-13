/*	$OpenBSD: setjmp.S,v 1.8 2015/09/13 07:36:58 guenther Exp $ */
/*
 * Copyright (c) 1996 Dale Rahn. All rights reserved.
 *
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

#include "SYS.h"
#include <machine/asm.h>

/* int setjmp(jmp_buf env) */

#define JMP_r1	0x04
#define JMP_r14	0x08
#define JMP_r15	0x0c
#define JMP_r16	0x10
#define JMP_r17	0x14
#define JMP_r18	0x18
#define JMP_r19	0x1c
#define JMP_r20	0x20
#define JMP_r21	0x24
#define JMP_r22	0x28
#define JMP_r23	0x2c
#define JMP_r24	0x30
#define JMP_r25	0x34
#define JMP_r26	0x38
#define JMP_r27	0x3c
#define JMP_r28	0x40
#define JMP_r29	0x44
#define JMP_r30	0x48
#define JMP_r31	0x4c
#define JMP_lr  0x50
#define JMP_cr  0x54
#define JMP_ctr	0x58
#define JMP_xer	0x5c
#define JMP_sig	0x60


.extern _libc_sigblock

ENTRY(setjmp)
	/* r31, mask */
	stw 31, JMP_r31(3)
	mflr 0
	stw 0, JMP_lr(3)
#ifdef __PIC__
	stw 30, JMP_r30(3)
	bcl 20, 31, 1f
1:	mflr 30
	addis 30, 30, _GLOBAL_OFFSET_TABLE_-1b@ha
	addi 30, 30, _GLOBAL_OFFSET_TABLE_-1b@l
#endif
	mr 31, 3
	li 3, 0
	bl PIC_PLT(_libc_sigblock)
	stw 3, JMP_sig(31)
	/* should sigaltstack be checked and saved */
	mr 3, 31
#ifdef __PIC__
	lwz 30, JMP_r30(3)
#endif
	lwz 0, JMP_lr(3)
	mtlr 0
	lwz 31, JMP_r31(3)
ENTRY(_setjmp)
	stw 31, JMP_r31(3)
	/* r1, r14-r30 */
	stw 1,  JMP_r1 (3)
	stw 14, JMP_r14(3)
	stw 15, JMP_r15(3)
	stw 16, JMP_r16(3)
	stw 17, JMP_r17(3)
	stw 18, JMP_r18(3)
	stw 19, JMP_r19(3)
	stw 20, JMP_r20(3)
	stw 21, JMP_r21(3)
	stw 22, JMP_r22(3)
	stw 23, JMP_r23(3)
	stw 24, JMP_r24(3)
	stw 25, JMP_r25(3)
	stw 26, JMP_r26(3)
	stw 27, JMP_r27(3)
	stw 28, JMP_r28(3)
	stw 29, JMP_r29(3)
	stw 30, JMP_r30(3)
	/* cr, lr, ctr, xer */
	mfcr 0
	stw 0, JMP_cr(3)
	mflr 0
	stw 0, JMP_lr(3)
	mfctr 0
	stw 0, JMP_ctr(3)
	mfxer 0
	stw 0, JMP_xer(3)
	/* f14-f31, fpscr */
	li 3, 0
	blr


.extern _libc_sigsetmask
ENTRY(longjmp)
	/* r31, mask */
	mr 29, 3
	mr 31, 4
#ifdef __PIC__
	mflr 0
	bcl 20, 31, 1f
1:	mflr 30
	addis 30, 30, _GLOBAL_OFFSET_TABLE_-1b@ha
	addi 30, 30, _GLOBAL_OFFSET_TABLE_-1b@l
	mtlr 0
#endif
	lwz 3, JMP_sig(3)
	bl PIC_PLT(_libc_sigsetmask)
	/* should we deal with sigonstack here ?? */
	mr 4, 31
	mr 3, 29
ENTRY(_longjmp)
	lwz 31, JMP_r31(3)
	/* r1, r14-r30 */
	lwz 1,  JMP_r1 (3)
	lwz 14, JMP_r14(3)
	lwz 15, JMP_r15(3)
	lwz 16, JMP_r16(3)
	lwz 17, JMP_r17(3)
	lwz 18, JMP_r18(3)
	lwz 19, JMP_r19(3)
	lwz 20, JMP_r20(3)
	lwz 21, JMP_r21(3)
	lwz 22, JMP_r22(3)
	lwz 23, JMP_r23(3)
	lwz 24, JMP_r24(3)
	lwz 25, JMP_r25(3)
	lwz 26, JMP_r26(3)
	lwz 27, JMP_r27(3)
	lwz 28, JMP_r28(3)
	lwz 29, JMP_r29(3)
	lwz 30, JMP_r30(3)
	/* cr, lr, ctr, xer */
	lwz 0, JMP_cr(3)
	mtcr 0
	lwz 0, JMP_lr(3)
	mtlr 0
	lwz 0, JMP_ctr(3)
	mtctr 0
	lwz 0, JMP_xer(3)
	mtxer 0
	/* f14-f31, fpscr */

	/* if r4 == 0, return 1, not 0 */
	mr	3, 4
	cmpwi	4,0
	bne	1f
	li	3, 1
1:
	blr
