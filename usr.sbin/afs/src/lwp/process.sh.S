/*	$OpenBSD: process.sh.S,v 1.6 2007/03/03 13:40:45 deraadt Exp $	*/

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

ENTRY(savecontext)

	mov.l	.L_PRE_Block, r0	/* PRE_Block = 1 */
	mov	#1, r1
	mov.l	r1, @r0

	/* sp == r15 - automatically handled */
	mov.l	r14, @-sp
	mov.l	r13, @-sp
	mov.l	r12, @-sp
	mov.l	r11, @-sp
	mov.l	r10, @-sp
	mov.l	r9, @-sp
	mov.l	r8, @-sp
	sts.l	pr, @-sp
	sts.l	macl, @-sp
	sts.l	mach, @-sp

#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	/* float point registers */
	sts	fpscr, r0
	mov.l	r0, @-sp
	mov	#0, r1
	sts.l	fpul, @-sp
	lds	r1, fpscr
	fmov.s	fr15, @-sp
	fmov.s	fr14, @-sp
	fmov.s	fr13, @-sp
	fmov.s	fr12, @-sp
	frchg
	fmov.s	fr15, @-sp
	fmov.s	fr14, @-sp
	fmov.s	fr13, @-sp
	fmov.s	fr12, @-sp
	lds	r0, fpscr
#endif

	mov.l	sp,@r5			/* store top of stack */
	tst	r6,r6
	bt	1f

	mov	r6,sp			/* swap to new sp */
	mov	sp, r14			/* frame pointer */
1:
	jmp	@r4
	 nop

	.align	2
.L_PRE_Block:	.long _C_LABEL(PRE_Block)
	SET_ENTRY_SIZE(savecontext)

/*
 * returnto(struct savearea *r4)
*/
ENTRY(returnto)
	mov.l	@r4, sp

#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	/* float point registers */
	mov	#0, r1
	lds	r1, fpscr
	frchg
	fmov.s	@sp+, fr12
	fmov.s	@sp+, fr13
	fmov.s	@sp+, fr14
	fmov.s	@sp+, fr15
	frchg
	fmov.s	@sp+, fr12
	fmov.s	@sp+, fr13
	fmov.s	@sp+, fr14
	fmov.s	@sp+, fr15
	lds.l	@sp+, fpul
	lds.l	@sp+, fpscr
#endif
	lds.l	@sp+, mach
	lds.l	@sp+, macl
	lds.l   @sp+, pr
	mov.l	@sp+, r8
	mov.l	@sp+, r9
	mov.l	@sp+, r10
	mov.l	@sp+, r11
	mov.l	@sp+, r12
	mov.l	@sp+, r13
	mov.l	@sp+, r14
	/* sp == r15 - automatically handled */

	mov.l	.L_PRE_Block2, r0	/* PRE_Block = 0 */
	mov	#0, r4
	mov.l	r4, @r0

	rts
	 nop

	.align	2
.L_PRE_Block2:	.long _C_LABEL(PRE_Block)
	SET_ENTRY_SIZE(returnto)
