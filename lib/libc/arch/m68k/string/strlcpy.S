/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DEFS.h"

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: strlcpy.S,v 1.4 1999/05/01 15:57:01 millert Exp $"
#endif /* LIBC_SCCS */

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
