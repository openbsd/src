/* $OpenBSD: bcopy_page.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: bcopy_page.S,v 1.1 1996/01/31 23:15:17 mark Exp $ */

/*
 * Copyright (c) 1995 Scott Stevens
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
 *	This product includes software developed by Scott Stevens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * RiscBSD kernel project
 *
 * bcopy_page.S
 *
 * page optimised bcopy and bzero routines
 *
 * Created      : 08/04/95
 * Last updated : 12/05/95
 *
 */

#include <machine/param.h>

/* TODO:
 *
 * Use r2 as the counter so that r3-r10 can be used instead of r4-r11
 * This means r11 will not need to be pushed on the stack.
 */

/* bcopy_page(src, dest)
 * r0 - src
 * r1 - dest
 * number of bytes (NBPG) is a multiple of 512
 */

	.global	_bcopy_page
_bcopy_page:
	mov	r3, #(NBPG >> 9)

	STMFD	r13!, {r4-r11, r14}

loopcopy:
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}

	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}

	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}

	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}
	ldmia	r0!, {r4-r11}
	stmia	r1!, {r4-r11}

	subs	r3, r3, #1
	bgt	loopcopy

	ldmfd	r13!, {r4-r11, r15}

/* bzero_page(dest)
 * r0 - dest
 * number of bytes is a multiple of 512
 */

	.global _bzero_page
_bzero_page:
	mov	r3, #(NBPG >> 9)

	stmfd	r13!, {r4-r11, r14}

	mov	r4, #0
	mov	r5, #0
	mov	r6, #0
	mov	r7, #0
	mov	r8, #0
	mov	r9, #0
	mov	r10, #0
	mov	r11, #0

loopzero:
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}

	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}

	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}

	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}
	stmia	r0!, {r4-r11}

	subs	r3, r3, #1
	bgt	loopzero

	ldmfd	r13!, {r4-r11, r15}
