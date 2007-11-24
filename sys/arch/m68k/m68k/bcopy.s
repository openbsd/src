/*	$OpenBSD: bcopy.s,v 1.3 2007/11/24 20:58:26 deraadt Exp $	*/
/*	$NetBSD: bcopy.s,v 1.1 1997/03/17 19:44:33 gwr Exp $	*/

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

/*
 * This is based on: src/lib/libc/arch/m68k/string/bcopy.S
 * identified as: @(#)bcopy.s        5.1 (Berkeley) 5/12/90
 */	

#include <machine/asm.h>

	.file	"bcopy.s"
	.text

/*
 * {ov}bcopy(from, to, len)
 * memcpy(to, from, len)
 *
 * Works for counts up to 128K.
 */
ALTENTRY(memmove, memcpy)
ENTRY(memcpy)
	movl	sp@(12),d0		| get count
	jeq	Lbccpyexit		| if zero, return
	movl	sp@(8), a0		| src address
	movl	sp@(4), a1		| dest address
	jra	Lbcdocopy		| jump into bcopy
ALTENTRY(ovbcopy, bcopy)
ENTRY(bcopy)
	movl	sp@(12),d0		| get count
	jeq	Lbccpyexit		| if zero, return
	movl	sp@(4),a0		| src address
	movl	sp@(8),a1		| dest address
Lbcdocopy:
	cmpl	a1,a0			| src before dest?
	jlt	Lbccpyback		| yes, copy backwards (avoids overlap)
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lbccfeven		| no, go check dest
	movb	a0@+,a1@+		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lbccpyexit		| exit if done
Lbccfeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lbccfbyte		| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lbccfbyte		| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lbccflloop:
	movl	a0@+,a1@+		| copy longwords
	dbf	d1,Lbccflloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lbccpyexit		| done if none
Lbccfbyte:
	subql	#1,d0			| set up for dbf
Lbccfbloop:
	movb	a0@+,a1@+		| copy bytes
	dbf	d0,Lbccfbloop		| til done
Lbccpyexit:
	rts
Lbccpyback:
	addl	d0,a0			| add count to src
	addl	d0,a1			| add count to dest
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lbccbeven		| no, go check dest
	movb	a0@-,a1@-		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lbccpyexit		| exit if done
Lbccbeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lbccbbyte		| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lbccbbyte		| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lbccblloop:
	movl	a0@-,a1@-		| copy longwords
	dbf	d1,Lbccblloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lbccpyexit		| done if none
Lbccbbyte:
	subql	#1,d0			| set up for dbf
Lbccbbloop:
	movb	a0@-,a1@-		| copy bytes
	dbf	d0,Lbccbbloop		| til done
	rts
