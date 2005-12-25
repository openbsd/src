/*	$OpenBSD: rfork_thread.S,v 1.1 2005/12/25 00:13:09 tedu Exp $ */
/*
 * Copyright (c) 2005 KUDO Takashi <takashi@crazyhack.net>
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
#include "../../../libc/arch/alpha/SYS.h"

/*
 *                        a0           a1            a2                  a3
 * int rfork_thread(int flags, void *stack, void (*func)(void *), void *arg);
 */
	.globl	rfork_thread
	.ent	rfork_thread
rfork_thread:
	LDGP(pv)
	lda	sp, -16(sp)
	stq	ra, 0(sp)
	.frame	sp, 16, ra
	.prologue 1

	CALLSYS_ERROR(rfork)
	beq 	v0, 1f

	/* parent process */
	ldq	ra, 0(sp)
	lda	sp, 16(sp)
	RET

1:
	/* child process */
	mov	a1, sp		/* setup stack */
	mov	a3, a0		
	jsr	ra, (a2), 0	/* funcall func(arg) */
	LDGP(ra)

	CALLSYS_ERROR(threxit)
	.end	rfork_thread
