/*	$OpenBSD: isa_io_asm.S,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: isa_io_asm.S,v 1.1 2002/02/10 12:26:01 chris Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mark Brinicombe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
 * bus_space I/O functions for isa
 */

#include <machine/asm.h>

#ifdef GPROF
#define	PAUSE	nop ; nop ; nop ; nop ; nop
#else
#define	PAUSE
#endif

/*
 * Note these functions use ARM Architecture V4 instructions as
 * all current systems with ISA will be using processors that support
 * V4 or later architectures (SHARK & CATS)
 */

/*
 * read single
 */

ENTRY(isa_bs_r_1)
	ldrb	r0, [r1, r2]
	PAUSE
	mov	pc, lr

ENTRY(isa_bs_r_2)
	ldrh	r0, [r1, r2]	/*.word	0xe19100b2*/
	PAUSE
	mov	pc, lr

ENTRY(isa_bs_r_4)
	ldr	r0, [r1, r2]
	PAUSE
	mov	pc, lr

/*
 * read multiple.
 */

ENTRY(isa_bs_rm_1)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rm_1_loop:
	ldrb	r3, [r0]
	strb	r3, [r1], #1
	subs	r2, r2, #1
	bne	Lisa_rm_1_loop

	mov	pc, lr

ENTRY(isa_bs_rm_2)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rm_2_loop:
	ldrh	r3, [r0]	/*.word 0xe1d030b0*/
	strh	r3, [r1], #2	/*.word 0xe0c130b2*/
	subs	r2, r2, #1
	bne	Lisa_rm_2_loop

	mov	pc, lr

ENTRY(isa_bs_rm_4)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rm_4_loop:
	ldr	r3, [r0]
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	Lisa_rm_4_loop

	mov	pc, lr

/*
 * read region.
 */

ENTRY(isa_bs_rr_1)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rr_1_loop:
	ldrb	r3, [r0], #1
	strb	r3, [r1], #1
	subs	r2, r2, #1
	bne	Lisa_rr_1_loop

	mov	pc, lr

ENTRY(isa_bs_rr_2)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rr_2_loop:
	ldrh	r3, [r0], #2
	strh	r3, [r1], #2	/*.word 0xe0c130b2*/
	subs	r2, r2, #1
	bne	Lisa_rr_2_loop

	mov	pc, lr

ENTRY(isa_bs_rr_4)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_rr_4_loop:
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	subs	r2, r2, #1
	bne	Lisa_rr_4_loop

	mov	pc, lr

/*
 * write single
 */

ENTRY(isa_bs_w_1)
	strb	r3, [r1, r2]
	PAUSE
	mov	pc, lr

ENTRY(isa_bs_w_2)
	strh	r3, [r1, r2]	/*.word 0xe18130b2*/
	PAUSE
	mov	pc, lr

ENTRY(isa_bs_w_4)
	str	r3, [r1, r2]
	PAUSE
	mov	pc, lr

/*
 * write multiple
 */
 
ENTRY(isa_bs_wm_1)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wm_1_loop:
	ldrb	r3, [r1], #1
	strb	r3, [r0]
	subs	r2, r2, #1
	bne	Lisa_wm_1_loop

	mov	pc, lr

ENTRY(isa_bs_wm_2)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wm_2_loop:	
	ldrh	r3, [r1], #2	/*.word 0xe0d130b2*/
	strh	r3, [r0]	/*.word 0xe1c030b0*/
	subs	r2, r2, #1
	bne	Lisa_wm_2_loop

	mov	pc, lr

ENTRY(isa_bs_wm_4)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wm_4_loop:
	ldr	r3, [r1], #4
	str	r3, [r0]
	subs	r2, r2, #1
	bne	Lisa_wm_4_loop

	mov	pc, lr


/*
 * write region.
 */

ENTRY(isa_bs_wr_1)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wr_1_loop:
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	subs	r2, r2, #1
	bne	Lisa_wr_1_loop

	mov	pc, lr

ENTRY(isa_bs_wr_2)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wr_2_loop:	
	ldrh	r3, [r1], #2	/*.word 0xe0d130b2*/
	strh	r3, [r0], #2
	subs	r2, r2, #1
	bne	Lisa_wr_2_loop

	mov	pc, lr

ENTRY(isa_bs_wr_4)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_wr_4_loop:
	ldr	r3, [r1], #4
	str	r3, [r0], #4
	subs	r2, r2, #1
	bne	Lisa_wr_4_loop

	mov	pc, lr

/*
 * Set region
 */

ENTRY(isa_bs_sr_2)
	add	r0, r1, r2
	mov	r1, r3
	ldr	r2, [sp, #0]
	teq	r2, #0
	moveq	pc, lr

Lisa_bs_sr_2_loop:
	strh	r1, [r0], #2		/*.word e0c010b2*/
	subs	r2, r2, #1
	bne	Lisa_bs_sr_2_loop

	mov	pc, lr
