/*	$OpenBSD: uthread_machdep_asm.S,v 1.2 2007/03/02 06:11:54 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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

/*
 * void _thread_machdep_switch(new, oldsave);
 */
ENTRY(_thread_machdep_switch)
	/*
	 * On entry: r4 = new, r5 = oldsave
	 */

	/*
	 * Save current context on the stack.
	 */
#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	sts.l	fpscr,	@-r15
	mov	#0,	r1
	sts.l	fpul,	@-r15
	lds	r1,	fpscr
	fmov.s	fr15,	@-r15	/* note that we can't do double stores... */
	fmov.s	fr14,	@-r15	/* ...as we don't control stack alignment. */
	fmov.s	fr13,	@-r15
	fmov.s	fr12,	@-r15
	frchg
	fmov.s	fr15,	@-r15
	fmov.s	fr14,	@-r15
	fmov.s	fr13,	@-r15
	fmov.s	fr12,	@-r15
#endif
	sts.l	mach,	@-r15
	sts.l	macl,	@-r15
	mov.l	r8,	@-r15
	mov.l	r9,	@-r15
	mov.l	r10,	@-r15
	mov.l	r11,	@-r15
	mov.l	r12,	@-r15
	mov.l	r13,	@-r15
	mov.l	r14,	@-r15
	sts.l	pr,	@-r15

	/*
	 * Switch stacks.
	 */
	mov.l	r15,	@r5
	mov.l	@r4,	r15

	/*
	 * Restore new context.
	 */
	lds.l	@r15+,	pr
	mov.l	@r15+,	r14
	mov.l	@r15+,	r13
	mov.l	@r15+,	r12
	mov.l	@r15+,	r11
	mov.l	@r15+,	r10
	mov.l	@r15+,	r9
	mov.l	@r15+,	r8
	lds.l	@r15+,	macl
#if defined(__SH4__) && !defined(__SH4_NOFPU__)
	mov	#0,	r1
	lds.l	@r15+,	mach
	lds	r1,	fpscr
	frchg
	fmov.s	@r15+,	fr12
	fmov.s	@r15+,	fr13
	fmov.s	@r15+,	fr14
	fmov.s	@r15+,	fr15
	frchg
	fmov.s	@r15+,	fr12
	fmov.s	@r15+,	fr13
	fmov.s	@r15+,	fr14
	fmov.s	@r15+,	fr15
	lds.l	@r15+,	fpul
	rts
	 lds.l	@r15+,	fpscr
#else
	rts
	 lds.l	@r15+,	mach
#endif
