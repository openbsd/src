/*	$OpenBSD: strlcpy.S,v 1.7 2005/08/07 16:40:14 espie Exp $ */
/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include "DEFS.h"

/*
 * It is faster to implement strlcpy() as two distinct operations,
 * a strlen() and a memcpy().  Note that we *always* add a NUL, unless
 * the count is 0.
 *
 * TODO: try unrolling the two main loops a bit
 */

ENTRY(strlcpy)
	movl	sp@(8),a0		| a0 = src
	movl	a0,a1			| working copy of src ptr
	movl	a0,d0			| return (d0) value is len(src)
	notl	d0			| take the complement of d0
strlcpy_len:
	tstb	a1@+			| did we hit the terminating NUL?
	bne	strlcpy_len		| nope, keep going
	addl	a1,d0
					| now set things up to do the copy
	movl	sp@(4),a1		| a1 = dst
	movl	sp@(12),d1		| d1 = count
	beq	strlcpy_done		| zero count, just return

	subql	#1,d1			| subtract one for the NUL
	cmpl	d0, d1			| must set d1 to MIN(d0, d1)
	ble	strlcpy_check		| count - 1 <= len(src), so d1 is ok
	movl	d0, d1			| d1 is too big, set it to len(src)
strlcpy_check:
	tstl	d1			| is d1 zero?
	beq	strlcpy_clear		| if so, skip copy and set byte to NUL
strlcpy_copy:
	movb	a0@+,a1@+		| copy a byte
	subql	#1,d1			| decrement count
	bne	strlcpy_copy		| space left?  continue copying...
strlcpy_clear:
	clrb	a1@			| clear last byte
strlcpy_done:
	rts
