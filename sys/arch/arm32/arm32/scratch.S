/* $OpenBSD: scratch.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: scratch.S,v 1.2 1996/02/02 02:35:49 mycroft Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 * scratch.S
 *
 * Miscellaneous routines scratchpad routines
 *
 * Created      : 02/04/95
 * Last updated : 04/06/95
 *
 */

#include "assym.h"

lr	.req	r14
pc	.req	r15

.text

        .global _ReadWordWithChecks

/* Reads a word but traps failed faults
 *
 *  r0 - address to read
 *  r1 - address to store result
 */

Lcurpcb:
	.word	_curpcb

_ReadWordWithChecks:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	ReadWordPCBFault
	add	r3, pc, #ReadWordFault - . - 8
	str	r3, [r2, #PCB_ONFAULT]

	ldr	r0, [r0]
	str	r0, [r1]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	pc, lr

ReadWordFault:
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	r0, #0x00000001
	mov	pc, lr

ReadWordPCBFault:
	stmfd	sp!, {lr}
	mov	r2, r0
	add	r0, pc, #ReadWordText1 - . - 8
	mov	r1, r2
	bl	_printf
	mov	r0, #0x00000001
	ldmfd	sp!, {lr}
	mov	pc, lr

ReadWordText1:
	.asciz	"Alert ! PCB = %08x during ReadWordWithChecks addr=%08x\n"


	.global	new_fuword

new_fuword:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r1, pc, #fusufault - . - 8
	str	r1, [r2, #PCB_ONFAULT]

	ldr	r0, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_fuswintr

new_fuswintr:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r1, pc, #fusubailout - . - 8
	str	r1, [r2, #PCB_ONFAULT]

	ldr	r0, [r0]
	bic	r0, r0, #0xff000000
	bic	r0, r0, #0x00ff0000

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_fusword

new_fusword:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r1, pc, #fusufault - . - 8
	str	r1, [r2, #PCB_ONFAULT]

	ldr	r0, [r0]
	bic	r0, r0, #0xff000000
	bic	r0, r0, #0x00ff0000

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_fubyte

new_fubyte:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r1, pc, #fusufault - . - 8
	str	r1, [r2, #PCB_ONFAULT]

	ldrb	r0, [r0]

	mov	r1, #0x00000000
	str	r1, [r2, #PCB_ONFAULT]
	mov	pc, lr


fusufault:
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	mov	pc, lr

	.global	fusubailout

fusubailout:
	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mvn	r0, #0x00000000
	mov	pc, lr

fusuaddrfault:
	mvn	r0, #0x00000000
	mov	pc, lr

fusupcbfault:
	stmfd	sp!, {lr}
	mov	r2, r0
	add	r0, pc, #ReadWordText1 - . - 8
	mov	r1, r2
	bl	_printf
	mvn	r0, #0x00000000
	ldmfd	sp!, {lr}
	mov	pc, lr

	.global	new_suword

new_suword:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r3, pc, #fusufault - . - 8
	str	r3, [r2, #PCB_ONFAULT]

	str	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_suswintr

new_suswintr:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r3, pc, #fusubailout - . - 8
	str	r3, [r2, #PCB_ONFAULT]

	strb	r1, [r0, #0x0000]
	mov	r1, r1, lsr #8
	strb	r1, [r0, #0x0000]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_susword

new_susword:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r3, pc, #fusufault - . - 8
	str	r3, [r2, #PCB_ONFAULT]

	strb	r1, [r0, #0x0000]
	mov	r1, r1, lsr #8
	strb	r1, [r0, #0x0000]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	pc, lr

	.global	new_subyte

new_subyte:
	ldr	r2, Lcurpcb
	ldr	r2, [r2]
	teq	r2, #0x00000000
	beq	fusupcbfault
	add	r3, pc, #fusufault - . - 8
	str	r3, [r2, #PCB_ONFAULT]

	strb	r1, [r0]

	mov	r0, #0x00000000
	str	r0, [r2, #PCB_ONFAULT]
	mov	pc, lr
