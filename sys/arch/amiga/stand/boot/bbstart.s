/*
 * $OpenBSD: bbstart.s,v 1.2 1997/05/13 16:17:46 niklas Exp $
 * $NetBSD: bbstart.s,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
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
 *
 */

#include "aout2bb/aout2bb.h"

#define LVOAllocMem	-0x0c6
#define LVODoIO		-0x1c8
#define LVOCacheClearU	-0x27c

#define IOcmd	28
#define IOerr	31
#define IOlen	36
#define IObuf	40
#define IOoff	44

#define Cmd_Rd	2

	.globl	_configure
	.globl	_pain

	.text
Lzero:	.asciz "DOS"			| "DOS type"
	/*
	 * We put the relocator version here, for aout2bb, which replaces
	 * it with the bootblock checksum.
	 */
Chksum:	.long RELVER_RELATIVE_BYTES_FORWARD
Filesz:	.long 8192			| dummy

/* 
 * Entry point from Kickstart.
 * A1 points to an IOrequest, A6 points to ExecBase, we have a stack.
 * _must_ be at offset 12.
 */
	.globl _start
_start:
#ifdef AUTOLOAD
	jra	Lautoload
#else
	jra	Lrelocate
#endif

Lreltab:
	.word 0			| aout2bb puts the reloc table address here

#ifdef AUTOLOAD
/*
 * autoload
 */
Lautoload:
	movl	a6,sp@-			|SysBase
	movl	a1,sp@-			|IORequest

	movl	#AUTOLOAD,d0		|Howmuch
	movl	d0,a1@(IOlen)		| for the actual read...
	movl	#0x10001,d1		|MEMF_CLEAR|MEMF_PUBLIC
	jsr	a6@(LVOAllocMem)
	movl	sp@+,a1			|IORequest
	movl	sp@+,a6			|SysBase
	orl	d0,d0
	jeq	Lerr
	movl	d0,sp@-			|Address
	movl	a1@(IOoff),sp@-		|Old offset
	movl	a1,sp@-
	movl	a6,sp@-

/* we've set IOlen above */
	movl	d0,a1@(IObuf)
	movw	#Cmd_Rd,a1@(IOcmd)
	jsr	a6@(LVODoIO)

	movl	sp@+,a6
	movl	sp@+,a1
	movl	sp@+,a1@(IOoff)

	tstb	a1@(IOerr)
	jne	Lioerr
	addl	#Lrelocate-Lzero,sp@

	movl	a6,sp@-
	jsr	a6@(LVOCacheClearU)
	movl	sp@+,a6
	rts
Lioerr:
	movql	#1,d0
	addql	#4,sp
	rts
#endif

/*
 * Relocate ourselves, at the same time clearing the relocation table
 * (in case it overlaps with BSS).
 *
 * Register usage:
 * A2: points into the reloc table, located at our end.
 * A0: pointer to the longword to relocate.
 * D0: word offset of longword to relocate
 * D1: points to our start.
 *
 * Table has relative byte offsets, if a byte offset is zero, it is
 * followed by an absolute word offset. If this is zero, too, table
 * end is reached.
 */
 
Lrelocate:
	lea	pc@(Lzero),a0
	movl	a0,d1
	movw	pc@(Lreltab),a2
	addl	d1,a2
	jra	Loopend
	
Loopw:
	clrw	a2@+
	movl	d1,a0	| for a variant with relative words, erase this line
Loopb:
	addl	d0,a0
	addl	d1,a0@
Loopend:
	movq	#0,d0
	movb	a2@,d0
	clrb	a2@+	| bfclr a2@+{0:8} is still two shorts
	tstb	d0	| we could save one short by using casb d0,d0,a2@+
	jne	Loopb

	movw	a2@,d0
	jne	Loopw

Lendtab:
	movl	a6,sp@-
	jsr	a6@(LVOCacheClearU)
	movl	sp@+,a6

/* We are relocated. Now it is safe to initialize _SysBase: */

	movl	a6,_SysBase

	movl	a1,sp@-
	bsr	_configure
	addql	#4,sp
	tstl	d0
	jne	Lerr

	bsr	_pain

Lerr:
	movql	#1,d0
	rts

	.comm _SysBase,4
