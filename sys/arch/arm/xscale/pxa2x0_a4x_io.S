/*	$NetBSD: pxa2x0_a4x_io.S,v 1.1 2002/10/19 19:31:39 bsh Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*	
 * There are simple bus space functions for IO registers mapped at
 * 32-bit aligned positions.  offset is multiplied by 4.  
 */

#include <machine/asm.h>

/*
 * bus_space I/O functions with offset*4
 */

/*
 * read single
 */

ENTRY(a4x_bs_r_1)
	ldrb	r0, [r1, r2, LSL #2]
	mov	pc, lr

ENTRY(a4x_bs_r_2)
	mov	r2, r2, LSL #2
	ldrh	r0, [r1, r2]
	mov	pc, lr

ENTRY(a4x_bs_r_4)
	ldr	r0, [r1, r2, LSL #2]
	mov	pc, lr

/*
 * write single
 */

ENTRY(a4x_bs_w_1)
	strb	r3, [r1, r2, LSL #2]
	mov	pc, lr

ENTRY(a4x_bs_w_2)
	mov	r2, r2, LSL #2
	strh	r3, [r1, r2]
	mov	pc, lr

ENTRY(a4x_bs_w_4)
	str	r3, [r1, r2, LSL #2]
	mov	pc, lr

/*
 * read multiple
 */
ENTRY(a4x_bs_rm_1)
	mov r2, r2, LSL #2
	b generic_bs_rm_1
	
ENTRY(a4x_bs_rm_2)
	mov r2, r2, LSL #2
	b generic_armv4_bs_rm_2



/*
 * write multiple
 */
ENTRY(a4x_bs_wm_1)
	mov r2, r2, LSL #2
	b generic_bs_wm_1
	
ENTRY(a4x_bs_wm_2)
	mov r2, r2, LSL #2
	b generic_armv4_bs_wm_2
