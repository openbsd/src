/* $OpenBSD: _main.S,v 1.2 2000/03/03 00:54:47 todd Exp $ */
/* $NetBSD: _main.S,v 1.1 1996/01/31 23:17:57 mark Exp $ */

/* Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES * LOSS OF USE, DATA, OR PROFITS * OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * _main.s
 *
 * Kernel entry/exit code
 *
 * Created      : 12/09/94
 * Last updated : 12/09/94
 *
 * Based on kate/boot/_main.s
 *
 */

#include "regs.h"

	.global	___main
___main:
	swi	0x20010
	mov	sp, r1
	sub	sl, sp, #2048

	ldr	r3, [pc, #cli_ptr - . - 8]
	str	r0, [r3]
#if 0
        LDR     a4,[pc,#|__cli_ptr|-.-8]
        STR     a1,[a4,#0]
        LDR     a4,[pc,#|__himem_ptr|-.-8]
        STR     a2,[a4,#0]
        LDR     a4,[pc,#|__stack_ptr|-.-8]
        STR     sl,[a4,#0]
        LDR     a4,[pc,#|__robase_ptr|-.-8]
        LDR     a1,[a4,#0]
        LDR     a4,[pc,#|__base_ptr|-.-8]
        STR     a1,[a4,#0]
        LDR     a4,[pc,#|__rwlimit_ptr|-.-8]
        LDR     a1,[a4,#0]
        LDR     a4,[pc,#|__lomem_ptr|-.-8]
        STR     a1,[a4,#0]
        LDR     a4,[pc,#|__break_ptr|-.-8]
        STR     a1,[a4,#0]
#endif
#if 0
	cmp	sl, r0
	movlss	pc, lr	/* no stack - exit fast */
	add	sl, sl, #256
#endif
	mov	fp, #0
	b	__main

cli_ptr:
	.word	___cli

	.data
	.global	___cli
___cli:
	.word	0

#if 0

|__cli_ptr|
        DCD     |__cli|

|__robase_ptr|
        DCD     |__robase|
|__base_ptr|
        DCD     |__base|
|__rwbase_ptr|
        DCD     |__rwbase|
|__rwlimit_ptr|
        DCD     |__rwlimit|
|__himem_ptr|
        DCD     |__himem|
|__lomem_ptr|
        DCD     |__lomem|

|__break_ptr|
        DCD     |__break|
|__stack_ptr|
        DCD     |__stack|

|__time_ptr|
        DCD     |__time|

        EXPORT  |__cli|         ; CLI from OS_GetEnv
|__cli|
        DCD     0

        EXPORT  |__base|        ; BASE (application = 0x8000)
|__base|
        DCD     0
        EXPORT  |__lomem|       ; LOMEM
|__lomem|
        DCD     0
        EXPORT  |__himem|       ; HIMEM from OS_GetEnv
|__himem|
        DCD     0

        EXPORT  |__break|       ; the 'break'
|__break|
        DCD     0
        EXPORT  |__stack|       ; stack limit
|__stack|
        DCD     0

#endif
