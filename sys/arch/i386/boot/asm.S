/*	$NetBSD: asm.S,v 1.12 1995/03/12 00:10:53 mycroft Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>
#define	addr32	.byte 0x67
#define	data32	.byte 0x66

CR0_PE		=	0x1

	.data
	.globl	_ourseg
_ourseg:
	.long	0

/*
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */
ENTRY(real_to_prot)
	# guarantee that interrupt is disabled when in prot mode
	cli

	# load the gdtr
	addr32
	data32
	lgdt	_C_LABEL(Gdtr)

	# set the PE bit of CR0
	movl	%cr0, %eax

	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	# make intrasegment jump to flush the processor pipeline and
	# reload CS register
	data32
	ljmp	$0x18, $xprot

xprot:
	# we are in USE32 mode now
	# set up the protected mode segment registers : DS, SS, ES
	movl	$0x20, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	ret

/*
 * prot_to_real()
 * 	transfer from protected mode to real mode
 */
ENTRY(prot_to_real)
	# set up a dummy stack frame for the second seg change.
	movl 	_ourseg, %eax
	pushw	%ax
	movl	$xreal, %eax	# gas botches pushw $xreal - extra bytes 0, 0
	pushw	%ax		# decode to add %al, (%eax) (%al usually 0)

	# Change to use16 mode.
	ljmp	$0x28, $x16

x16:
	# clear the PE bit of CR0
	movl	%cr0, %eax
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0

	# make intersegment jmp to flush the processor pipeline
	# using the fake stack frame set up earlier
	# and reload CS register
	lret

xreal:
	# we are in real mode now
	# set up the real mode segment registers : DS, SS, ES
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	sti
	data32
	ret

/*
 * startprog(phyaddr)
 *	start the program on protected mode where phyaddr is the entry point
 */
ENTRY(startprog)
	pushl	%ebp
	movl	%esp, %ebp

	# get things we need into registers
	movl	8(%ebp), %ecx		# entry offset 
	movl	12(%ebp), %eax		# &argv

	# make a new stack at 0:0x90000 (big segs)
	movl	$0x10, %ebx
	movw	%bx, %ss
	movl	$0x90000, %ebx
	movl	%ebx, %esp
	
	# push some number of args onto the stack
	pushl	28(%eax)		# argv[7] = cnvmem
	pushl	32(%eax)		# argv[8] = extmem
	pushl	16(%eax)		# argv[4] = esym
	pushl	$0			# nominally a cyl offset in the boot.
	pushl	8(%eax)			# argv[2] = bootdev
	pushl	4(%eax)			# argv[1] = howto
	pushl	$0			# dummy 'return' address
	
	# push on our entry address
	movl	$0x8, %ebx		# segment
	pushl	%ebx
	pushl	%ecx

	# convert over the other data segs
	movl	$0x10, %ebx
	movl	%bx, %ds
	movl	%bx, %es

	# convert the PC (and code seg)
	lret

/*
 * pbzero(dst, cnt)
 *	where dst is a virtual address and cnt is the length
 */
ENTRY(pbzero)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$0x10, %eax
	movl	%ax, %es

	movl	8(%ebp), %edi		# destination
	movl	12(%ebp), %ecx		# count
	xorl	%eax, %eax		# value

	rep
	stosb

	popl	%edi
	popl	%es
	popl	%ebp
	ret

/*
 * pcpy(src, dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */
ENTRY(pcpy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%esi
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$0x10, %eax
	movl	%ax, %es

	movl	8(%ebp), %esi		# source
	movl	12(%ebp), %edi		# destination
	movl	16(%ebp), %ecx		# count

	rep
	movsb

	popl	%edi
	popl	%esi
	popl	%es
	popl	%ebp
	ret

#ifdef CHECKSUM
/*
 * cksum(src, cnt)
 *	where src is a virtual address and cnt is the length
 */
ENTRY(cksum)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es
	pushl	%edi

	cld

	# set %es to point at the flat segment
	movl	$0x10, %eax
	movl	%ax, %es

	movl	8(%ebp), %edi		# destination
	movl	12(%ebp), %ecx		# count
	shrl	$2, %ecx
	xorl	%edx, %edx		# value

1:	es
	lodsl
	xorl	%eax, %edx
	loop	1b

	movl	%edx, %eax

	popl	%edi
	popl	%es
	popl	%ebp
	ret
#endif

#if 0
ENTRY(getword)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%es

	# set %es to point at the flat segment
	movl	$0x10, %eax
	movl	%ax, %es

	movl	8(%ebp), %eax
	es
	movl	(%eax), %edx

	movl	%edx, %eax

	popl	%es
	popl	%ebp
	ret
#endif
