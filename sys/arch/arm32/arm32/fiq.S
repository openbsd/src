/* $OpenBSD: fiq.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: fiq.S,v 1.1 1996/01/31 23:15:55 mark Exp $ */

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
 * fiq.S
 *
 * Low level fiq handlers
 *
 * Created      : 27/09/94
 * Last updated : 28/11/94
 *
 */

#include <machine/cpu.h>

sp	.req	r13
lr	.req	r14
pc	.req	r15

.text

	.global	_floppy_read_fiq
	.global	_floppy_read_fiq_end

_floppy_read_fiq:
	subs	r11, r11, #0x00000001
	addeq	r13, r13, #0x00018000
	ldrb	r10, [r13]
	strb	r10, [r12], #0x0001
	subs	pc, lr, #0x00000004
_floppy_read_fiq_end:

	.global	_floppy_write_fiq
	.global	_floppy_write_fiq_end

_floppy_write_fiq:
	subs	r11, r11, #0x00000001
	addeq	r13, r13, #0x00018000
	ldrb	r10, [r12], #0x0001
	strb	r10, [r13]
	subs	pc, lr, #0x00000004

_floppy_write_fiq_end:
