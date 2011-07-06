/*	$OpenBSD: start.S,v 1.3 2011/07/06 18:32:59 miod Exp $ */
/*	$NetBSD: start.S,v 1.1 2002/02/24 01:04:26 matt Exp $ */
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
		

#define _LOCORE

#include "sys/disklabel.h"

#include "../include/mtpr.h"
#include "../include/asm.h"		

_C_LABEL(_start):
_C_LABEL(start):
	.globl _C_LABEL(start)	# this is the symbolic name for the start
	.globl _C_LABEL(_start) # of code to be relocated. We can use this
				# to get the actual/real address (pc-rel)
				# or to get the relocated address (abs).

.org	0x00			# uVAX booted from TK50 starts here
	brb	from_0x00	# continue behind dispatch-block

.org	0x02			# information used by uVAX-ROM
	.byte	0xff		# offset in words to identification area 
	.byte	1		# this byte must be 1
	.word	0		# logical block number (word swapped) 
	.word	0		# of the secondary image

.org	0x08			#
	brb	from_0x08	# skip ...

.org	0x0C			# 11/750  & 8200 starts here
	halt			# not supported


from_0x00:			# uVAX from TK50 
	brw	start_uvax	# all uVAXen continue there

from_0x08:			# Any machine from VMB
	movzbl	$4,_C_LABEL(from)		# Booted from full VMB
	brw	start_vmb

# the complete area reserved for label
# must be empty (i.e. filled with zeroes).
# disklabel(8) checks that before installing
# the bootblocks over existing label.

.org	LABELOFFSET
	.globl	_C_LABEL(romlabel)
_C_LABEL(romlabel):
	.long	0

.org	LABELOFFSET + d_end_
start_vmb:
	/*
	 * Read in block 1-15.
	 */
	movl	52(r11), r7	# load iovec/bqo into %r7
	addl3	(r7), r7, r6	# load qio into %r6
	pushl	r11		# base of rpb
	pushl	$0		# virtual-flag 
	pushl	$33		# read-logical-block
	pushl	$1		# lbn to start reading
	pushl	$7680		# number of bytes to read
	pushab	start_uvax	# buffer-address 
	calls	$6, (r6)	# call the qio-routine
	brw	start_uvax

/*
 * Parameter block for uVAX boot.
 */
#define VOLINFO		0	/* 1=single-sided  81=double-sided volumes */
#define SISIZE		16	/* size in blocks of secondary image */
#define SILOAD		0	/* load offset (usually 0) from the default */
#define SIOFF		0x260	/* byte offset into secondary image */

.org	0x1fe
	.byte	0x18		# must be 0x18 
	.byte	0x00		# must be 0x00 (MBZ) 
	.byte	0x00		# any value 
	.byte	0xFF - (0x18 + 0x00 + 0x00)	
		/* 4th byte holds 1s' complement of sum of previous 3 bytes */

	.byte	0x00		# must be 0x00 (MBZ) 
	.byte	VOLINFO
	.byte	0x00		# any value 
	.byte	0x00		# any value 

	.long	SISIZE		# size in blocks of secondary image 
	.long	SILOAD		# load offset (usually 0) 
	.long	SIOFF		# byte offset into secondary image 
	.long	(SISIZE + SILOAD + SIOFF)	# sum of previous 3 


	.align	2
	.globl	_C_LABEL(from)
_C_LABEL(from):
	.long	0

/* uVAX main entry is at the start of the second disk block.  This is
 * needed for multi-arch CD booting where multiple architecture need
 * to shove stuff in boot block 0.
 */
	.org	0x260		# uVAX booted from disk starts here

start_uvax:
	movzbl	$2,_C_LABEL(from)	# Booted from subset-VMB
	brb	start_all

/*
 * start_all: stack already at RELOC, we save registers, move ourself
 * to RELOC and loads boot.
 */
start_all:
	movl	$_C_LABEL(start), sp	# move stack to a better 
	pushr	$0x1fff			# save all regs, used later.

	subl3	$_C_LABEL(start), $_C_LABEL(edata), r0
					# get size of text+data (w/o bss)
	moval	_C_LABEL(start), r1	# get actual base-address of code
	subl3	$_C_LABEL(start), $_C_LABEL(end), r2
					# get complete size (incl. bss)
	movl	$_C_LABEL(start), r3	# get relocated base-address of code
	movc5	r0, (r1), $0, r2, (r3)	# copy code to new location
	
	movpsl	-(sp)
	movl	$relocated, -(sp)	# return-address on top of stack 
	rei				# can be replaced with new address
relocated:				# now relocation is done !!!
	movl	sp, _C_LABEL(bootregs)
	calls	$0, _C_LABEL(Xmain)	# call Xmain (gcc workaround)which is 
	halt				# not intended to return ...

/*
 * hoppabort() is called when jumping to the newly loaded program.
 */
ENTRY(hoppabort, 0)
	movl    4(ap),r6
	movl	_C_LABEL(rpb),r11
	mnegl	$1,ap		# Hack to figure out boot device.
	movpsl	-(sp)
	pushab	2(r6)
	mnegl	$1,_C_LABEL(vax_load_failure)
	rei
#	calls	$0,(r6)
	halt

ENTRY(unit_init, R6|R7|R8|R9|R10|R11)
	mfpr	$17,r7			# Wanted bu KDB
	movl	4(ap),r0		# init routine address
	movl	8(ap),r9		# RPB in %r9
	movl	12(ap),r1		# VMB argument list
	callg	(r1),(r0)
	ret

# A bunch of functions unwanted in boot blocks.
ENTRY(getchar, 0)
	halt

#ifndef USE_PRINTF
ENTRY(putchar, 0)
	ret

#endif

ENTRY(panic, 0)
	halt
