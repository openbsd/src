/*	$OpenBSD: rfork_thread.S,v 1.3 2012/01/17 02:34:18 guenther Exp $	*/

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

#include "../../../libc/arch/sh/SYS.h"

/*
 * int __tfork_thread(const struct __tfork *param, void *stack, void (*func)(void *), void *arg);
 *			r4			r5		   r6		r7
 */
ENTRY(__tfork_thread)
	mov.l	.LSYS___tfork, r0
	.word	0xc380		/* trapa #0x80 */
	bf	9f

	tst	r0, r0
	bt	1f

	/*
	 * In parent process: just return.
	 */
	rts
	 nop

1:
	/*
	 * In child process: switch stack, invoke function, then exit.
	 */
	mov	r5, sp
	jsr	@r6
	 mov	r7, r4

	mov.l	.LSYS___threxit, r0
	.word	0xc380		/* trapa #0x80 */

9:
	/*
	 * System call failure.
	 */
	JUMP_CERROR

	.align 2
.LSYS___tfork:	.long SYS___tfork
.LSYS___threxit:	.long SYS___threxit

	SET_ENTRY_SIZE(__tfork_thread)
