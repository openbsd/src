/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2007/02/19 21:03:50 miod Exp $	*/

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

	/* save caller-saved context on stack */
	mov.l	r8,	@-r15
	mov.l	r9,	@-r15
	mov.l	r10,	@-r15
	mov.l	r11,	@-r15
	mov.l	r12,	@-r15
	mov.l	r13,	@-r15
	mov.l	r14,	@-r15
	sts.l	pr,	@-r15

	/* save old stack */
	mov.l	r15,	@r5

	/* switch stacks */
	mov.l	@r4,	r15

	/* restore new context */
	lds.l	@r15+,	pr
	mov.l	@r15+,	r14
	mov.l	@r15+,	r13
	mov.l	@r15+,	r12
	mov.l	@r15+,	r11
	mov.l	@r15+,	r10
	mov.l	@r15+,	r9
	rts
	 mov.l	@r15+,	r8
