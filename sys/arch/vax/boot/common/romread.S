/*	$OpenBSD: romread.S,v 1.1 2002/06/11 09:36:23 hugh Exp $ */
/*	$NetBSD: romread.S,v 1.1 2002/02/24 01:04:25 matt Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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
 * read750 (int block, int *regs)
 */
ENTRY(read750, 0xFFE)
	movl	8(ap), r8
	movl	4(r8), r1
	movl	8(r8), r2
	movl	12(r8), r3
	movl	24(r8), r6
	clrl	r5
	movl	4(ap), r8
	pushl	$0
	movl	$0, 4(sp)
	movl    fp, 0xf0000	# ragge ???
	jsb	(r6)
	movl    0xf0000, fp	
	ret

/*
 * romread_uvax (int lbn, int size, void *buf, struct rpb *rpb)
 */
ENTRY(romread_uvax, 0xFFE)
	movl	16(ap),r11	# restore boot-contents of %r11 (rpb)
	movl    52(r11), r7     # load iovec/bqo into %r7
	addl3   (r7), r7, r6	# load qio into %r6
	pushl	r11		# base of rpb
	pushl	$0		# virtual-flag 
	pushl	$33		# read-logical-block
	pushl	4(ap)		# lbn to start reading
	pushl	8(ap)		# number of bytes to read
	pushl	12(ap)		# buffer-address 
	calls	$6, (r6)	# call the qio-routine
	ret			# %r0 holds the result

/*
 * romwrite_uvax (int lbn, int size, void *buf, struct rpb *rpb)
 */
ENTRY(romwrite_uvax, 0xFFE)
	movl    16(ap), r11	# restore boot-contents of %r11 (rpb)
	movl    52(r11), r7     # load iovec/bqo into %r7
	addl3   (r7), r7, r6    # load qio into %r6
	pushl   r11             # base of rpb
	pushl   $0              # virtual-flag 
	pushl   $32             # write-logical-block
	pushl   4(ap)           # lbn to start reading
	pushl   8(ap)           # number of bytes to read
	pushl   12(ap)          # buffer-address 
	calls   $6, (r6)        # call the qio-routine
	ret                     # %r0 holds the result

