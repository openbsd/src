/*	$NetBSD: start.s,v 1.7 1996/02/02 19:08:33 mycroft Exp $ */
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
		

#define	_LOCORE

#include "sys/disklabel.h"

#include "../include/mtpr.h"
#include "../include/asm.h"		

_start:	.globl _start		# this is the symbolic name for the start
				# of code to be relocated. We can use this
				# to get the actual/real adress (pc-rel)
				# or to get the relocated address (abs).

.org	0x00			# uVAX booted from TK50 starts here
	brb	from_0x00	# continue behind dispatch-block

.org	0x02			# information used by uVAX-ROM
	.byte (LABELOFFSET + d_end_)/2 # offset in words to identification area 
	.byte	1		# this byte must be 1
	.word	0		# logical block number (word swapped) 
	.word	0		# of the secondary image

.org	0x08			#
	brb	from_0x08	# skip ...

.org	0x0A			# uVAX booted from disk starts here
	brb	from_0x0A	# skip ...

.org	0x0C			# 11/750  & 8200 starts here
	brw	cont_750


from_0x00:			# uVAX from TK50 
from_0x0A:			# uVAX from disk
	brw	start_uvax	# all(?) uVAXen continue there

from_0x08:			# What comes here???
	halt

.org	LABELOFFSET - 6
regmask: 	.word 0x0fff	# using a variable saves 3 bytes !!!
bootinfo:	.long 0x0	# another 3 bytes if within byte-offset

# the complete area reserved for label
# must be empty (i.e. filled with zeroes).
# disklabel(8) checks that before installing
# the bootblocks over existing label.

/*
 * Parameter block for uVAX boot.
 */
#define VOLINFO         0       /* 1=single-sided  81=double-sided volumes */
#define SISIZE          16      /* size in blocks of secondary image */
#define SILOAD          0       /* load offset (usually 0) from the default */
#define SIOFF           0x0A    /* byte offset into secondary image */

.org    LABELOFFSET + d_end_
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
	.long 	SIOFF		# byte offset into secondary image 
	.long	(SISIZE + SILOAD + SIOFF)	# sum of previous 3 

/*
 * After bootblock (LBN0) has been loaded into the first page 
 * of good memory by 11/750's ROM-code (transfer address
 * of bootblock-code is: base of good memory + 0x0C) registers
 * are initialized as:
 * 	R0:	type of boot-device
 *			0:	Massbus device
 *			1:	RK06/RK07
 *			2:	RL02
 *			17:	UDA50
 *			35:	TK50
 *			64:	TU58
 *	R1:	(UBA) address of UNIBUS I/O-page
 *		(MBA) address of boot device's adapter
 * 	R2:	(UBA) address of the boot device's CSR
 *		(MBA) controller number of boot device
 *	R6:	address of driver subroutine in ROM
 *
 * cont_750 reads in LBN1-15 for further execution.
 */
	.align 2
cont_750:
        movl    r0,r10
        movl    r5, ap	# ap not used here
        clrl    r5
        clrl    r4
        movl    $_start,sp
1:      incl    r4
        movl    r4,r8
        addl2   $0x200,r5
        cmpl    $16,r4
        beql    2f
        pushl   r5
        jsb     (r6)
        blbs    r0,1b
2:      movl	r10, r0
	movl	r11, r5
	brw	start_all


start_uvax:
	mtpr	$0, $PR_MAPEN	# Turn off MM, please.
	movl    $_start, sp
	movl	48(r11), ap
	brb	start_all

/*
 * start_all: stack already at RELOC, we save registers, move ourself
 * to RELOC and loads boot.
 */
start_all:
	pushr	$0xfff			# save all regs, used later.

	subl3	$_start, $_edata, r0	# get size of text+data (w/o bss)
	moval	_start, r1		# get actual base-address of code
	subl3	$_start, $_end, r2	# get complete size (incl. bss)
	movl	$_start, r3		# get relocated base-address of code
	movc5	r0, (r1), $0, r2, (r3)	# copy code to new location
	
	movl	$relocated, -(sp)	# return-address on top of stack 
	rsb	 			# can be replaced with new address
relocated:				# now relocation is done !!!
	movl	sp, _bootregs
	movl	ap, _boothowto
	calls	$0, _main		# call main() which is 
	halt				# not intended to return ...

/*
 * hoppabort() is called when jumping to the newly loaded program.
 */
ENTRY(hoppabort, 0)
        movl    4(ap),r6
        movl    8(ap),r11
        movl    0xc(ap),r10
        calls   $0,(r6)
	halt
