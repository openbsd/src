/* $OpenBSD: areahand.S,v 1.2 2000/03/03 00:54:47 todd Exp $ */
/* $NetBSD: areahand.S,v 1.1 1996/01/31 23:18:01 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
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
 * SERVICES; LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * areahand.s
 *
 *
 *
 * Created      : 12/09/94
 * Last updated : 12/09/94
 *
 */

#include "regs.h"

	.text
	.global	_kernarea_handler
/*
 * r0 - code
 * r1 - page block
 * r2 - entries
 * r3 - size increase
 * r4 - current size
 * r5 - pagesize
 * r12 - arrangement table
 */

_kernarea_handler:
/* Is it a pregrow request ? */
	teq	r0, #0x00000000
	bicnes	pc, lr, #1<<28

/* Make sure that the area is currently 0 in size */
	teq	r4, #0x00000000
	movne	r0, #0x000000
	orrnes	pc, lr, #1<<28

	teq	r1, #0x00000000
	biceqs	pc, lr, #1<<28

/* Store registers */
	stmfd	sp!, {r0-r12}

/* Get first page number */
	ldr	r8, [r12, #0x0004]
	sub	r8, r8, r2
	mov	r8, #0x00000700
	str	r1, [r12]

loop:
	str	r8, [r1], #0x000c
	add	r8, r8, #0x00000001
	subs	r2, r2, #0x00000001
	bne	loop

	ldmfd	sp!, {r0-r12}
/*	bics	pc, lr, #1<<28 */
	adr	r0, error1
	orrs	pc, lr, #1<<28

error1:
	.word	0
	.asciz	"error1"
