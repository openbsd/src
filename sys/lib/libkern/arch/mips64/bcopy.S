/*	$OpenBSD: bcopy.S,v 1.2 2004/08/13 13:07:24 pefo Exp $	*/
/*-
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 * memcpy(to, from, len)
 * {ov}bcopy(from, to, len)
 */
LEAF(memcpy)
	.set	noreorder
	move	v0, a0			# swap from and to
	move	a0, a1
	move	a1, v0
ALEAF(bcopy)
ALEAF(ovbcopy)
	.set	noreorder
	addu	t0, a0, a2		# t0 = end of s1 region
	sltu	t1, a1, t0
	sltu	t2, a0, a1
	and	t1, t1, t2		# t1 = true if from < to < (from+len)
	beq	t1, zero, forward	# non overlapping, do forward copy
	slt	t2, a2, 12		# check for small copy

	ble	a2, zero, 2f
	addu	t1, a1, a2		# t1 = end of to region
1:
	lb	v1, -1(t0)		# copy bytes backwards,
	subu	t0, t0, 1		#   doesnt happen often so do slow way
	subu	t1, t1, 1
	bne	t0, a0, 1b
	sb	v1, 0(t1)
2:
	j	ra
	nop
forward:
	bne	t2, zero, smallcpy	# do a small bcopy
	xor	v1, a0, a1		# compare low two bits of addresses
	and	v1, v1, 3
	subu	a3, zero, a1		# compute # bytes to word align address
	beq	v1, zero, aligned	# addresses can be word aligned
	and	a3, a3, 3

	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	LWHI	v1, 0(a0)		# get next 4 bytes (unaligned)
	LWLO	v1, 3(a0)
	addu	a0, a0, a3
	SWHI	v1, 0(a1)		# store 1, 2, or 3 bytes to align a1
	addu	a1, a1, a3
1:
	and	v1, a2, 3		# compute number of words left
	subu	a3, a2, v1
	move	a2, v1
	addu	a3, a3, a0		# compute ending address
2:
	LWHI	v1, 0(a0)		# copy words a0 unaligned, a1 aligned
	LWLO	v1, 3(a0)
	addu	a0, a0, 4
	sw	v1, 0(a1)
	addu	a1, a1, 4
	bne	a0, a3, 2b
	nop				# We have to do this mmu-bug.
	b	smallcpy
	nop
aligned:
	beq	a3, zero, 1f
	subu	a2, a2, a3		# subtract from remaining count
	LWHI	v1, 0(a0)		# copy 1, 2, or 3 bytes to align
	addu	a0, a0, a3
	SWHI	v1, 0(a1)
	addu	a1, a1, a3
1:
	and	v1, a2, 3		# compute number of whole words left
	subu	a3, a2, v1
	move	a2, v1
	addu	a3, a3, a0		# compute ending address
2:
	lw	v1, 0(a0)		# copy words
	addu	a0, a0, 4
	sw	v1, 0(a1)
	bne	a0, a3, 2b
	addu	a1, a1, 4
smallcpy:
	ble	a2, zero, 2f
	addu	a3, a2, a0		# compute ending address
1:
	lbu	v1, 0(a0)		# copy bytes
	addu	a0, a0, 1
	sb	v1, 0(a1)
	bne	a0, a3, 1b
	addu	a1, a1, 1	# MMU BUG ? can not do -1(a1) at 0x80000000!!
2:
	j	ra
	nop
END(memcpy)
