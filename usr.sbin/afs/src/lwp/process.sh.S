/*	$OpenBSD: process.sh.S,v 1.1 2006/11/15 00:22:39 deraadt Exp $	*/

/*
 * Copyright (c) 2006 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#undef RCSID

#include <machine/asm.h>

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
#	r4             r5                      r6
*/

ENTRY(_savecontext)
	mov.l	r4, @-sp
	mov.l	r0, @-sp		/* save on stack */
	mov.l	r15, @-sp
	mov.l	r14, @-sp
	mov.l	r13, @-sp
	mov.l	r12, @-sp
	mov.l	r11, @-sp
	mov.l	r10, @-sp
	mov.l	r9, @-sp
	mov.l	r8, @-sp
	sts.l	pr, @-sp

	mov.l	.L_PRE_Block, r0	/* PRE_Block = 1 */
	mov	#1, r4
	mov.l	r4, @r0

	mov.l	sp,@r5			/* store top of stack */
	mov.l	@r6,sp			/* swap to new sp */

	mov.l	r4,@-sp			/* call function */
	rts
	 nop

	.align	2
.L_PRE_Block:	.long _C_LABEL(PRE_Block) - _C_LABEL(_savecontext)
	SET_ENTRY_SIZE(_savecontext)

/*
 * returnto(struct savearea *a2)
*/
ENTRY(_returnto)
	mov	r4, sp

	lds.l	@r4+, pr
	mov.l	@r4+, r8
	mov.l	@r4+, r9
	mov.l	@r4+, r10
	mov.l	@r4+, r11
	mov.l	@r4+, r12
	mov.l	@r4+, r13
	mov.l	@r4+, r14
	mov.l	@r4+, r15

	mov.l	.L_PRE_Block2, r0	/* PRE_Block = 1 */
	mov	#0, r4
	mov.l	r4, @r0

	mov.l	@sp+, r0
	mov.l	@sp+, r4
	
	rts
	 nop

	.align	2
.L_PRE_Block2:	.long _C_LABEL(PRE_Block) - _C_LABEL(_returnto)
	SET_ENTRY_SIZE(_savecontext)
