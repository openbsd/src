/* $OpenBSD: bcopyinout.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: bcopyinout.S,v 1.3 1996/02/02 18:05:47 mycroft Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * bcopyinout.S
 *
 * optimized and fault protected copyinout function
 *
 * Created      : 16/05/95
 * Last updated : 16/05/95
 *
 */

#include "assym.h"
#include <sys/errno.h>

sp	.req	r13
lr	.req	r14
pc	.req	r15

.text
Lcurpcb:
	.word	_curpcb

Lcurproc:
	.word	_curproc

bcopyinoutfault:
	mov	r0, #0x00000000
	str	r0, [r4, #PCB_ONFAULT]
	mov	r0, #EFAULT
	ldmfd	sp!, {r4}
	mov	pc, lr

bcopyinoutpcbfault:
	stmfd	sp!, {lr}
	mov	r3, r1
	mov	r1, r4
	add	r0, pc, #bcopyinouttext - . - 8
	mov	r2, r3
	ldr	r3, Lcurproc
	ldr	r3, [r3]
	bl	_printf
	mov	r0, #EFAULT
	ldmfd	sp!, {lr}
	ldmfd	sp!, {r4}
	mov	pc, lr

bcopyinouttext:
	.asciz	"Alert ! PCB = %08x during bcopyinout addr=%08x curproc=%08x\n"

	.align	0

	.global _bcopyinout

_bcopyinout:
	teq	r2, #0x00000000
	moveq	r0, #0x000000000
	moveq	pc,lr

	stmfd	sp!, {r4}
	ldr	r4, Lcurpcb
	ldr	r4, [r4]
	teq	r4, #0x00000000
	beq	bcopyinoutpcbfault
	add	r3, pc, #bcopyinoutfault - . - 8
	str	r3, [r4, #PCB_ONFAULT]

	cmp	r0, r1
	blt	bcopy_back

bcopy_loop:
	ldrb	r3, [r0], #0x0001
	strb	r3, [r1], #0x0001
	subs	r2, r2, #0x00000001
	bne	bcopy_loop

	mov	r0, #0x00000000
	str	r0, [r4, #PCB_ONFAULT]
	ldmfd	sp!, {r4}
	mov	pc, lr

bcopy_back:
	add	r0, r0, r2
	add	r1, r1, r2

bcopy_bloop:
	ldrb	r3, [r0, #-0x0001]!
	strb	r3, [r1, #-0x0001]!
	subs	r2, r2, #0x00000001
	bne	bcopy_bloop

	mov	r0, #0x00000000
	str	r0, [r4, #PCB_ONFAULT]
	ldmfd	sp!, {r4}
	mov	pc, r14

