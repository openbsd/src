/*	$OpenBSD: memset.S,v 1.5 2005/08/07 16:40:14 espie Exp $ */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#include "DEFS.h"

/*
 * This is probably not the best we can do, but it is still much
 * faster than the C version in the portable gen directory.
 *
 * Things that might help:
 *	- unroll the longword loop (might not be good for a 68020)
 *	- longword, as opposed to word, align when possible (only on the 68020)
 *	- use nested DBcc instructions or use one and limit size to 64K
 */
ENTRY(memset)
	movl	d2,sp@-
	movl	sp@(8),a0		| destination
	movl	sp@(16),d0		| count
	beq	bzdone			| nothing to do
	movb	sp@(15),d2		| character
	movl	a0,d1
	btst	#0,d1			| address odd?
	beq	bzeven			| no, skip alignment
	movb	d2,a0@+			| set one byte
	subql	#1,d0			| adjust count
	beq	bzdone			| if zero, all done
bzeven:
	cmpl	#15,d0
	ble	bzbloop			| too small to be worthwhile
	clrl	d1			| replicate byte to fill longword
	movb	d2,d1
	movl	d1,d2
	lsll	#8,d1
	orl	d1,d2
	lsll	#8,d1
	orl	d1,d2
	lsll	#8,d1
	orl	d1,d2
	movl	d0,d1			| convert to longword count
	lsrl	#2,d1
#ifdef DEBUG
	moveml	#0xffff,sp@-
	movl	d2,sp@-
	movl	d1,sp@-
	movl	d0,sp@-
	movl	a0,sp@-
	movl	#foo,sp@-
	jsr	_printf
	addl	#20,sp
	moveml	sp@+,#0xffff
#endif
bzlloop:
	movl	d2,a0@+			| set one longword
	subql	#1,d1			| adjust count
	bne	bzlloop			| still more, keep going
	andl	#3,d0			| what remains
	beq	bzdone			| nothing, all done
bzbloop:
	movb	d2,a0@+			| set one byte
	subql	#1,d0			| adjust count
	bne	bzbloop			| still more, keep going
bzdone:
	movl	sp@(8),d0		| return destination
	movl	sp@+,d2
	rts

#ifdef DEBUG
	.globl	_printf
foo:
	.asciz	"a0=%08x d0=%08x d1=%08x d2=%08x\n"
#endif
