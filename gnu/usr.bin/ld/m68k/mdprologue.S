/* *	$OpenBSD: mdprologue.S,v 1.2 1998/03/26 19:47:03 niklas Exp $*/
/*
 * Copyright (c) 1993 Paul Mackerras
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
 *      This product includes software developed by Paul Mackerras.
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

/*
 * Run-time link editor entry points for m68k architecture.
 */

	.globl	_rtl
_rtl:
	link	a6,#0
	movel	a5,sp@-
	movel	a6@(12),a0			| -> crt_ldso structure
	movel	a0@,a1				| base address
	movel	#__GLOBAL_OFFSET_TABLE_,a5	| PIC function prologue
	lea	pc@(0,a5:l),a5
	movel	a5@,d0				| 1st entry in GOT
	addl	a1,d0				| add ld.so base address
	movel	d0,sp@-				| gives address of __DYNAMIC
	movel	a0,sp@-				| crt_ldso pointer
	movel	a6@(8),sp@-			| version
	addl	a5@(_rtld:w),a1			| relocate _rtld function
	jsr	a1@				| call it
|	movel	#0x80000004,d0			| CC_EXTPURGE | CC_IPURGE
|	trap	#12				| cachectl
	movel	a6@(-4),a5
	unlk	a6
	rts

	.globl	_binder_entry
_binder_entry:
	movml	a0-a1/d0-d1,sp@-	| preserve scratch registers
	movl	sp@(16),a0		| grab return address (within PLT)
	moveq	#0,d0
	movw	a0@,d0			| get function index
	movel	d0,sp@-
	subql	#6,a0			| point to PLT slot
	movel	a0,sp@-
	jbsr	_binder			| relocate function address
	addql	#8,sp
	movel	d0,sp@(16)		| put function address on stack
|	movel	#0x80000004,d0		| CC_EXTPURGE | CC_IPURGE
|	trap	#12			| cachectl
	movml	sp@+,a0-a1/d0-d1	| restore scratch registers
	rts				| jump into function (via address on stack)

	.globl	__cachectl
__cachectl:
	movel	sp@(4),a1		| address
	movel	sp@(8),d1		| length
	movel	#0x80000004,d0		| function = CC_EXTPURGE | CC_IPURGE
	trap	#12			| cachectl "syscall"
	rts
