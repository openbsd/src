/*	$NetBSD: bsdstart.s,v 1.1.1.1 1996/01/07 21:50:49 leo Exp $	*/

/*
 * Copyright (c) 1995 L. Weppelman
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This function sets up the registers according to the kernel parameter block,
 * disables the MMU and jumps to the kernel.
 *
 * bsd_startup(struct kparamb *)
 */
	.text
	.even
	.globl	_bsd_startup

_bsd_startup:
	movw	#0x2700,sr

	| the BSD kernel wants values into the following registers:
	| d0:  ttmem-size
	| d1:  stmem-size
	| d2:  cputype
	| d3:  boothowto
	| d4:  length of loaded kernel
	| d5:  start of fastram
	| a0:  start of loaded kernel
	| a1:  end of symbols (esym)
	| All other registers zeroed for possible future requirements.

	movl	sp@(4), a3		| a3 points to parameter block
#ifndef	STANDALONE
	lea	_bsd_startup,sp		| make sure we have a good stack ***
#endif
	movl	a3@,a0			| loaded kernel
	movl	a3@(8),d0		| kernel entry point
	addl	a0,d0			| added makes our absolute entry point
	movl	d0,sp@-			| push entry point		***
	movl	a3@(12),d1		| stmem-size
	movl	a3@(16),d0		| ttmem-size
	movl	a3@(20),d2		| bootflags
	movl	a3@(24),d3		| boothowto
	movl	a3@(4),d4		| length of loaded kernel
	movl	a3@(28),d5		| start of fastram
	movl	a3@(32),a1		| end of symbols
	subl	a5,a5			| target, load to 0
	btst	#4, d2			| Is this an 68040?
	beq	not040

	| Turn off 68040 MMU
	.word 0x4e7b,0xd003		| movec a5,tc
	.word 0x4e7b,0xd806		| movec a5,urp
	.word 0x4e7b,0xd807		| movec a5,srp
	.word 0x4e7b,0xd004		| movec a5,itt0
	.word 0x4e7b,0xd005		| movec a5,itt1
	.word 0x4e7b,0xd006		| movec a5,dtt0
	.word 0x4e7b,0xd007		| movec a5,dtt1
	bra	nott

not040:
	lea	pc@(zero),a3
	pmove	a3@,tc			| Turn off MMU
	pmove	a3@(-4),crp		| crp = nullrp
	pmove	a3@(-4),srp		| srp = nullrp

	| Turn off 68030 TT registers
	btst	#3, d2			| Is this an 68030?
	jeqs	nott
	.word	0xf013,0x0800		| pmove	a3@,tt0
	.word	0xf013,0x0c00		| pmove	a3@,tt1

nott:
	movq	#0,d6			|  would have known contents)
	movl	d6,d7
	movl	d6,a2
	movl	d6,a3
	movl	d6,a4
	movl	d6,a5
	movl	d6,a6
	rts				| enter kernel at address on stack ***


| A do-nothing MMU root pointer (includes the following long as well)
| Note that the above code makes assumptions about the order of the following
| items.

nullrp:	.long	0x80000202
zero:	.long	0
#ifndef	STANDALONE
svsp:	.long   0
#endif
