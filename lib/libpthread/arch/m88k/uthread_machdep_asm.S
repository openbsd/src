/*	$OpenBSD: uthread_machdep_asm.S,v 1.2 2004/03/02 23:41:29 miod Exp $	*/

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

/*
 * void _thread_machdep_switch(new, oldsave);
 */
ENTRY(_thread_machdep_switch)
	subu	r31, r31, 4 * 28

	/* save all registers but r0, r2, r3 and r31 on stack */
	st.d	r4,  r31, 4 * 0
	st.d	r6,  r31, 4 * 2
	st.d	r8,  r31, 4 * 4
	st.d	r10, r31, 4 * 6
	st.d	r12, r31, 4 * 8
	st.d	r14, r31, 4 * 10
	st.d	r16, r31, 4 * 12
	st.d	r18, r31, 4 * 14
	st.d	r20, r31, 4 * 16
	st.d	r22, r31, 4 * 18
	st.d	r24, r31, 4 * 20
	st.d	r26, r31, 4 * 22
	st.d	r28, r31, 4 * 24
	st	r30, r31, 4 * 26
	st	r1,  r31, 4 * 27
	
	/* exchange stacks */
	st	r31, r3,  0	/* oldsave->frame = r31 */
	ld	r31, r2,  0	/* r31 = oldsave->frame */

	/* restore registers */
	ld.d	r4,  r31, 4 * 0
	ld.d	r6,  r31, 4 * 2
	ld.d	r8,  r31, 4 * 4
	ld.d	r10, r31, 4 * 6
	ld.d	r12, r31, 4 * 8
	ld.d	r14, r31, 4 * 10
	ld.d	r16, r31, 4 * 12
	ld.d	r18, r31, 4 * 14
	ld.d	r20, r31, 4 * 16
	ld.d	r22, r31, 4 * 18
	ld.d	r24, r31, 4 * 20
	ld.d	r26, r31, 4 * 22
	ld.d	r28, r31, 4 * 24
	ld	r30, r31, 4 * 26
	ld	r1,  r31, 4 * 27

	jmp.n	r1
	 addu	r31, r31, 4 * 28
