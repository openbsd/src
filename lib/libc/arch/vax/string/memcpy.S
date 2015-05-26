/*	$OpenBSD: memcpy.S,v 1.8 2015/05/26 20:23:39 miod Exp $ */
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * void *memcpy(dst, src, size)
 * returns dst
 *
 * This optimises the usual case (count < 65536) at the expense
 * of some extra memory references and branches when count >= 65536.
 */

#include "DEFS.h"

ENTRY(memcpy, R2|R3|R4|R5)
	movzwl	$65535,%r0	/* r0 = 64K (needed below) */
	movq	8(%ap),%r1	/* r1 = src, r2 = length */
	movl	4(%ap),%r3	/* r3 = dst */
1:
	/* copy forward */
	cmpl	%r2,%r0
	bgtru	3f		/* stupid movc3 limitation */
	movc3	%r2,(%r1),(%r3)	/* move it all */
2:
	movl	4(%ap),%r0	/* return original dst */
	ret
3:
	subl2	%r0,12(%ap)	/* adjust length by 64K */
	movc3	%r0,(%r1),(%r3)	/* move 64K */
	movl	12(%ap),%r2
	decw	%r0		/* from 0 to 65535 */
	brb	1b		/* retry */
