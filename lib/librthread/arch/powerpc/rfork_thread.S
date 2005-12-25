/*	$OpenBSD: rfork_thread.S,v 1.2 2005/12/25 00:46:08 tedu Exp $	*/

/*
 * Copyright (c) 2005 Tim Wiess <tim@nop.cx>
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

#include <sys/syscall.h>
#include <machine/asm.h>

ENTRY(rfork_thread)
	/* sanity check */
	cmpwi	%r4, 0
	beq	1f
	cmpwi	%r5, 0
	beq	1f
	
	mr	%r7,%r4
	
	/* call rfork */
	li	%r0, SYS_rfork
	sc
	cmpwi	%r0, 0
	bne	2f
	
	/* check if we are parent or child */
	cmpwi	%r3, 0
	bnelr
	
	/* child */
	mtlr	%r5		/* fp */
	mr	%r3, %r6	/* arg */
	mr	%r1, %r7	/* new sp */
	blrl
	
	/* child returned, call _exit */
	li	%r0, SYS_threxit
	sc
1:
	li	%r3, -1
2:
	b PIC_PLT(_C_LABEL(__cerror))
