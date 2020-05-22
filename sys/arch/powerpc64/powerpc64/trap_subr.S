/* $OpenBSD: trap_subr.S,v 1.1 2020/05/22 15:07:47 kettenis Exp $ */
/* $NetBSD: trap_subr.S,v 1.20 2002/04/22 23:20:08 kleink Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"

#include <machine/psl.h>
#include <machine/trap.h>

	.abiversion 2

#define GET_TOCBASE(r) \
	ld	r, TRAP_TOCBASE(0)

	.text

	.globl trapcode, trapcodeend
trapcode:
	mtsprg1	%r1
	mflr	%r1
	mtsprg2	%r1
	ld	%r1, TRAP_ENTRY(0)
	mtlr	%r1
	li	%r1, 0xe0
	blrl
trapcodeend:

	.globl generictrap
generictrap:
	/* Save R1 for computing the exception vector */
	mtsprg3	%r1

	/* Compute the exception vector from the link register */
	mfsprg3 %r31
	ori	%r31, %r31, 0xff00
	mflr	%r30
	addi	%r30, %r30, -4 /* The branch instruction, not the next */
	and	%r30, %r30, %r31
	mtsprg3	%r30

	mfsprg1	%r1
	mfmsr	%r30
	ori	%r30, %r30, PSL_RI@l
	mtmsr	%r30
	mfsprg1	%r31
	stdu	%r31, -(FRAMELEN+288)(%r1)
	std	%r31, FRAME_1+48(%r1)
	mfsprg3	%r5
	std	%r5, FRAME_EXC+48(%r1)

	GET_TOCBASE(%r2)

	addi	%r3, %r1, 48
	bl	trap
