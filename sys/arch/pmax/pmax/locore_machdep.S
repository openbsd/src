/*	$NetBSD: locore_machdep.S,v 1.3 1996/10/22 20:46:09 mhitch Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *
 *	@(#)locore.s	8.5 (Berkeley) 1/4/94
 */


/*
 * DEcstation-specific mips locore code.
 */

#include <machine/asm.h>

	.set	noreorder

/*
 * Copy data to a DMA buffer padded with 16 bits of data, 16
 * bits of padding per 32bit word (e.g., for pmin/pmax  sii (and lance) DMA).
 *
 * The DMA bufffer can only be written one short at a time
 * (and takes ~14 cycles).
 *
 *	CopyToBuffer(src, dst, length)
 *		u_short *src;	NOTE: must be short aligned
 *		u_short *dst;
 *		int length;
 */
LEAF(CopyToBuffer)
	blez	a2, 2f
	nop
1:
	lhu	t0, 0(a0)		# read 2 bytes of data
	subu	a2, a2, 2
	addu	a0, a0, 2
	addu	a1, a1, 4
	bgtz	a2, 1b
	sh	t0, -4(a1)		# write 2 bytes of data to buffer
2:
	j	ra
	nop
END(CopyToBuffer)

/*
 * Copy data from the DMA buffer.
 * The DMA bufffer can only be read one short at a time
 * (and takes ~12 cycles).
 *
 *	CopyFromBuffer(src, dst, length)
 *		u_short *src;
 *		char *dst;
 *		int length;
 */
LEAF(CopyFromBuffer)
	and	t0, a1, 1		# test for aligned dst
	beq	t0, zero, 3f
	nop
	blt	a2, 2, 7f		# at least 2 bytes to copy?
	nop
1:
	lhu	t0, 0(a0)		# read 2 bytes of data from buffer
	addu	a0, a0, 4		# keep buffer pointer word aligned
	addu	a1, a1, 2
	subu	a2, a2, 2
	sb	t0, -2(a1)
	srl	t0, t0, 8
	bge	a2, 2, 1b
	sb	t0, -1(a1)
3:
	blt	a2, 2, 7f		# at least 2 bytes to copy?
	nop
6:
	lhu	t0, 0(a0)		# read 2 bytes of data from buffer
	addu	a0, a0, 4		# keep buffer pointer word aligned
	addu	a1, a1, 2
	subu	a2, a2, 2
	bge	a2, 2, 6b
	sh	t0, -2(a1)
7:
	ble	a2, zero, 9f		# done?
	nop
	lhu	t0, 0(a0)		# copy one more byte
	nop
	sb	t0, 0(a1)
9:
	j	ra
	nop
END(CopyFromBuffer)

/*
 * Interrupt counters for vmstat.
 */
	.data
	.globl _C_LABEL(intrcnt)
	.globl _C_LABEL(eintrcnt)
	.globl _C_LABEL(intrnames)
	.globl _C_LABEL(eintrnames)
_C_LABEL(intrnames):
	.asciiz	"softclock"
	.asciiz	"softnet"
	.asciiz	"serial0"
	.asciiz	"serial1"
	.asciiz	"serial2"
	.asciiz	"ether"
	.asciiz	"scsi"
	.asciiz	"memory"
	.asciiz	"clock"
	.asciiz	"fpu"
	.asciiz	"tcslot0"
	.asciiz	"tcslot1"
	.asciiz	"tcslot2"
	.asciiz "dtop"
	.asciiz "isdn"
	.asciiz	"floppy"
	.asciiz	"stray"
	.asciiz	"nmi"
	.asciiz	"lostclock"
_C_LABEL(eintrnames):
	.align	2
_C_LABEL(intrcnt):
	.word	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0
_C_LABEL(eintrcnt):
	.word	0	# This shouldn't be needed but with 4.4bsd's as, the eintrcnt
			#  label ends end up in a different section otherwise.
