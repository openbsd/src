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
	.asciz "$OpenBSD: strlcpy.S,v 1.1 1999/04/25 00:10:06 millert Exp $"
#endif /* LIBC_SCCS */

ENTRY(strlcpy)
	movl	sp@(4),a0		| a0 = dst
	movl	sp@(8),a1		| a1 = src
	movl	a1,a2			| a2 = orig_src
	movl	sp@(12),a3		| a3 = orig_count
	movl	a3,d1			| d1 = count
	beq	slcloop			| don't let count wrap...
	subql	#1,d1			| subtract one for the NUL
slcloop:
	tstb	a1@			| got a NUL?
	beq	slcnul			| if so, add NUL to dst and we're done
	tstl	d1			| only copy bytes if count is non-zero
	beq	slcinc			| (else just increment src ptr)
	movb	a1@,a0@+		| copy a byte but only increment dst
	subql	#1,d1			| decrement count
slcinc:
	addql	#1,a1			| increment src ptr
	jra	slcloop			| keep going...
slcnul:
	tstl	a3			| make sure we had a non-zero orig_count
	beq	slcdone
	clrb	a0@			| clear last byte
slcdone:
	movl	a1,d0
	subl	a2,d0			| return value
	rts
