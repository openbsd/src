/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2004/02/27 22:19:46 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/asm.h>

	global	__thread_machdep_switch
	.type	__thread_machdep_switch, @function
__thread_machdep_switch:
	subu    r31,r31,4*30

	st	r1,r31,4*29
	st	r3,r31,4*0
	st	r4,r31,4*1
	st	r5,r31,4*2
	st	r6,r31,4*3
	st	r7,r31,4*4
	st	r8,r31,4*5
	st	r9,r31,4*6
	st	r10,r31,4*7
	st	r11,r31,4*8
	st	r12,r31,4*9
	st	r13,r31,4*10
	st	r14,r31,4*11
	st	r15,r31,4*12
	st	r16,r31,4*13
	st	r17,r31,4*14
	st	r18,r31,4*15
	st	r19,r31,4*16
	st	r20,r31,4*17
	st	r21,r31,4*18
	st	r22,r31,4*19
	st	r23,r31,4*20
	st	r24,r31,4*21
	st	r25,r31,4*22
	st	r26,r31,4*23
	st	r27,r31,4*24
	st	r28,r31,4*25
	st	r29,r31,4*26
	st	r30,r31,4*27
	
	st	r31,r0,r3
	ld	r31,r0,r2

	ld	r1,r31,4*29
	ld	r3,r31,4*0
	ld	r4,r31,4*1
	ld	r5,r31,4*2
	ld	r6,r31,4*3
	ld	r7,r31,4*4
	ld	r8,r31,4*5
	ld	r9,r31,4*6
	ld	r10,r31,4*7
	ld	r11,r31,4*8
	ld	r12,r31,4*9
	ld	r13,r31,4*10
	ld	r14,r31,4*11
	ld	r15,r31,4*12
	ld	r16,r31,4*13
	ld	r17,r31,4*14
	ld	r18,r31,4*15
	ld	r19,r31,4*16
	ld	r20,r31,4*17
	ld	r21,r31,4*18
	ld	r22,r31,4*19
	ld	r23,r31,4*20
	ld	r24,r31,4*21
	ld	r25,r31,4*22
	ld	r26,r31,4*23
	ld	r27,r31,4*24
	ld	r28,r31,4*25
	ld	r29,r31,4*26
	ld	r30,r31,4*27

	jmp.n	r1
	addu    r31,r31,4*30

	.size	__thread_machdep_switch , . - __thread_machdep_switch
	.align 8
