/*	$OpenBSD: srt0.s,v 1.1 2000/04/27 02:26:26 bjc Exp $ */
/*	$NetBSD: srt0.s,v 1.2 1999/05/23 21:58:19 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */

#include "../include/asm.h"
/*
 * Auto-moving startup code for standalone programs. Can be loaded
 * (almost) anywhere in memory but moves itself to the position
 * it is linked for. Must be started at first position, recommended
 * is phys addr 0 (boot loads programs at 0, but starts them at the
 * position set in a.out header.
 */

nisse:	.set	nisse,0		# pass -e nisse to ld gives OK start addr
	.globl	nisse

_start:	.globl	_start
	nop;nop;		# If we get called by calls, or something

	movl	r8, _memsz	# If we come from disk, save memsize
	cmpl	ap, $-1		# Check if we are net-booted. XXX - kludge
	beql	2f		# jump if not
	ashl	$9,76(r11),_memsz # got memsize from rpb
	movzbl	102(r11), r10	# Get bootdev from rpb.
	movzwl	48(r11), r11	# Get howto

2:	movl	$_start, sp	# Probably safe place for stack
	subl2	$52, sp		# do not overwrite saved boot-registers

	subl3	$_start, $_edata, r0
	movab	_start, r1
	movl	$_start, r3
	movc3	r0,(r1),(r3)	# Kopiera text + data
	subl3	$_edata, $_end, r2
	movc5	$0,(r3),$0,r2,(r3) # Nolla bss också.

	jsb	1f
1:	movl    $relocated, (sp)   # return-address on top of stack
	rsb                        # can be replaced with new address
relocated:	                   # now relocation is done !!!
	movl	r10,_bootdev	# Save bootdev early
	movl	r11,_howto	# howto also...
	movl	sp, _bootregs
	calls	$0, _Xmain	# Were here!
	halt			# no return

ENTRY(machdep_start, 0)
	mtpr	$0x1f,$0x12	# Block all interrupts
	mtpr	$0,$0x18	# stop real time interrupt clock
	movl	4(ap), r6
	movl	_howto, r11
	movl	_opendev, r10
	movl	20(ap), r9
	movl	_memsz, r8
	calls	$0,(r6)
	ret

	.globl	_memsz
_memsz:	.long	0x0
