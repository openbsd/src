/*
 * Copyright (c) 1995 Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "DEFS.h"

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: _setjmp.S,v 1.3 1996/08/19 08:11:29 tholo Exp $"
#endif

	.text
	.align	0

ENTRY(_setjmp)
/*        LDR     a4,[pc,#|__fpflag_ptr|-.-8]
        LDR     a4,[a4,#0]
        CMP     a4,#0
        ADDEQ   a1,a1,#48
        BEQ     |__setjmp_l1|
        STFE    f4,[a1],#12
        STFE    f5,[a1],#12
        STFE    f6,[a1],#12
        STFE    f7,[a1],#12
|__setjmp_l1|*/
	add	r0, r0, #48
        stmia	r0, {r1-r12, r13, r14}
        mov	r0, #0x00000000
        mov	r15, r14

ENTRY(_longjmp)
	ldr	r2, L_longjmpret
	str	r1, [r2]
/*        LDR     a4,[pc,#|__fpflag_ptr|-.-8]
        LDR     a4,[a4,#0]
        CMP     a4,#0
        ADDEQ   a1,a1,#48
        BEQ     |__longjmp_l1|
        LDFE    f4,[a1],#12
        LDFE    f5,[a1],#12
        LDFE    f6,[a1],#12
        LDFE    f7,[a1],#12
|__longjmp_l1|*/
	add	r0, r0, #48
        ldmia	r0, {r1-r12, r13, r14}
	ldr	r0, L_longjmpret
	ldr	r0, [r0]
	teq	r0, #0x00000000
	moveq	r0, #0x00000001
	mov	r15, r14

L_longjmpret:
	.word	_longjmpret

	.data
	.align	0

_longjmpret:
	.word	0x00000000
