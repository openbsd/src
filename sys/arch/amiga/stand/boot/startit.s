/*	$OpenBSD: startit.s,v 1.2 1997/03/25 17:05:55 niklas Exp $	*/
/*	$NetBSD: startit.s,v 1.1.1.1 1996/11/29 23:36:29 is Exp $	*/

/*
 * Copyright (c) 1996 Ignatios Souvatzis
 * Copyright (c) 1994 Michael L. Hitch
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
 *      This product includes software developed by Michael L. Hitch.
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

	.set	ABSEXECBASE,4

	.text

	.globl	_startit
	.globl	_startit_end

_startit:
#if TESTONAMIGA
	movew	#0x999,0xdff180		| gray
#endif
	movel	sp,a3
	movel	4:w,a6
	lea	pc@(start_super:w),a5
	jmp	a6@(-0x1e)		| supervisor-call

start_super:
#if TESTONAMIGA
	movew	#0x900,0xdff180		| dark red
#endif
	movew	#0x2700,sr

	| the BSD kernel wants values into the following registers:
	| a0:  fastmem-start
	| d0:  fastmem-size
	| d1:  chipmem-size
	| d3:  Amiga specific flags
	| d4:  E clock frequency
	| d5:  AttnFlags (cpuid)
	| d7:  boothowto
	| a4:  esym location
	| a2:  Inhibit sync flags
	| All other registers zeroed for possible future requirements.

	lea	pc@(_startit:w),sp	| make sure we have a good stack ***

	movel	a3@(4),a1		| loaded kernel
	movel	a3@(8),d2		| length of loaded kernel
|	movel	a3@(12),sp		| entry point in stack pointer
	movel	a3@(12),a6		| entry point		***
	movel	a3@(16),a0		| fastmem-start
	movel	a3@(20),d0		| fastmem-size
	movel	a3@(24),d1		| chipmem-size
	movel	a3@(28),d7		| boothowto
	movel	a3@(32),a4		| esym
	movel	a3@(36),d5		| cpuid
	movel	a3@(40),d4		| E clock frequency
	movel	a3@(44),d3		| Amiga flags
	movel	a3@(48),a2		| Inhibit sync flags
	movel	a3@(52),d6		| Load to fastmem flag
	subl	a5,a5			| target, load to 0

	cmpb	#0x7D,a3@(36)		| is it DraCo?
	beq	nott			| yes, switch off MMU later

					| no, it is an Amiga:

#if TESTONAMIGA
	movew	#0xf00,0xdff180		|red
#endif
|	moveb	#0,0x200003c8
|	moveb	#63,0x200003c9
|	moveb	#0,0x200003c9
|	moveb	#0,0x200003c9

	movew	#(1<<9),0xdff096	| disable DMA on Amigas.

| ------ mmu off start -----

	btst	#3,d5			| AFB_68040,SysBase->AttnFlags
	beq	not040

| Turn off 68040/060 MMU

	subl	a3,a3
	.word 0x4e7b,0xb003		| movec a3,tc
	.word 0x4e7b,0xb806		| movec a3,urp
	.word 0x4e7b,0xb807		| movec a3,srp
	.word 0x4e7b,0xb004		| movec a3,itt0
	.word 0x4e7b,0xb005		| movec a3,itt1
	.word 0x4e7b,0xb006		| movec a3,dtt0
	.word 0x4e7b,0xb007		| movec a3,dtt1
	bra	nott

not040:
	lea	pc@(zero:w),a3
	pmove	a3@,tc			| Turn off MMU
	lea	pc@(nullrp:w),a3
	pmove	a3@,crp			| Turn off MMU some more
	pmove	a3@,srp			| Really, really, turn off MMU

| Turn off 68030 TT registers

	btst	#2,d5			| AFB_68030,SysBase->AttnFlags
	beq	nott			| Skip TT registers if not 68030
	lea	pc@(zero:w),a3
	.word 0xf013,0x0800		| pmove a3@,tt0 (gas only knows about 68851 ops..)
	.word 0xf013,0x0c00		| pmove a3@,tt1 (gas only knows about 68851 ops..)

nott:
| ---- mmu off end ----
#if TESTONAMIGA
	movew	#0xf60,0xdff180		| orange
#endif
|	moveb	#0,0x200003c8
|	moveb	#63,0x200003c9
|	moveb	#24,0x200003c9
|	moveb	#0,0x200003c9

| ---- copy kernel start ----

	tstl	d6			| Can we load to fastmem?
	beq	L0			| No, leave destination at 0
	movl	a0,a5			| Move to start of fastmem chunk
	addl	a0,a6			| relocate kernel entry point
L0:
	movl	a1@+,a5@+
	subl	#4,d2
	bcc	L0

	lea	pc@(ckend:w),a1
	movl	a5,sp@-
	pea	pc@(_startit_end:w)
L1:
	movl	a1@+,a5@+
	cmpl	sp@,a1
	bcs	L1
	addql	#4,sp

	btst	#3,d5
	jeq	L2
	.word	0xf4f8
L2:	movql	#0,d2			| switch off cache to ensure we use
	movec	d2,cacr			| valid kernel data

#if TESTONAMIGA
	movew	#0xFF0,0xdff180		| yellow
#endif
|	moveb	#0,0x200003c8
|	moveb	#63,0x200003c9
|	moveb	#0,0x200003c9
|	moveb	#0,0x200003c9

	rts

| ---- copy kernel end ----

ckend:
#if TESTONAMIGA
	movew	#0x0ff,0xdff180		| petrol
#endif
|	moveb	#0,0x200003c8
|	moveb	#0,0x200003c9
|	moveb	#63,0x200003c9
|	moveb	#63,0x200003c9

	movl	d5,d2
	roll	#8,d2
	cmpb	#0x7D,d2
	jne	noDraCo

| DraCo: switch off MMU now:

	subl	a3,a3
	.word 0x4e7b,0xb003		| movec a3,tc
	.word 0x4e7b,0xb806		| movec a3,urp
	.word 0x4e7b,0xb807		| movec a3,srp
	.word 0x4e7b,0xb004		| movec a3,itt0
	.word 0x4e7b,0xb005		| movec a3,itt1
	.word 0x4e7b,0xb006		| movec a3,dtt0
	.word 0x4e7b,0xb007		| movec a3,dtt1
	
noDraCo:
	moveq	#0,d2			| zero out unused registers
	moveq	#0,d6			| (might make future compatibility
	movel	d6,a1			|  would have known contents)
	movel	d6,a3
	movel	d6,a5
	movel	a6,sp			| entry point into stack pointer
	movel	d6,a6

#if TESTONAMIGA
	movew	#0x0F0,0xdff180		| green
#endif
|	moveb	#0,0x200003c8
|	moveb	#0,0x200003c9
|	moveb	#63,0x200003c9
|	moveb	#0,0x200003c9

	jmp	sp@			| jump to kernel entry point


| A do-nothing MMU root pointer (includes the following long as well)

nullrp:	.long	0x7fff0001
zero:	.long	0

_startit_end:
