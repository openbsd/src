/*	$NetBSD: asm.s,v 1.3 1994/10/27 04:21:05 cgd Exp $	*/

/*
 * source in this file came from
 * the 386BSD boot blocks written by Julian Elischer.
 *
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 */

	.file "asm.s"

#include "asm.h"

	.text

ENTRY(StartProg)
	/* void StartProg(u_long phyaddr, u_long *args)
	 * start the program in protected mode.  phyaddr is the entry point.
	 */

#ifndef USE_BUFFER
	push	%ebp
	mov	%esp, %ebp

	# get things we need into registers
	movl	0x8(%ebp), %ecx		# entry offset 
	movl	0x0c(%ebp), %eax	# &argv

	# make a new stack at 0:0xa0000 (big segs)
	mov	$KERN_DATA_SEG, %ebx
	movw	%bx, %ss
	movl	$0xa0000,%ebx
	movl	%ebx,%esp
	
	
	# push some number of args onto the stack
	pushl	$0			# nominally a cyl offset in the boot.
	pushl	0x8(%eax)		# argv[2] = bootdev
	pushl	0x4(%eax)		# argv[1] = howto
	pushl	$0			# dummy 'return' address
	
	# push on our entry address
	mov	$KERN_CODE_SEG, %ebx		# segment
	pushl	%ebx
	pushl	%ecx

	# convert over the other data segs
	mov	$KERN_DATA_SEG, %ebx
	movw	%bx, %ds
	movw	%bx, %es

	# convert the PC (and code seg)
	lret
#else
/* test version - relocating, kernel move (TBD) */
	/* if necessary, move ourself out of the way
	 * move the copy of the kernel to its correct load point
	 * set up a stack and transfer args to it
	 * call the entry point
	 * this is best done in assembly as the potential relocation will
	 * have a bad effect on non-PIC code.
	 */
	/* get things we need into registers */
	movl	0x8(%ebp), %ecx		# entry offset 
	movl	0x0c(%ebp), %eax	# &argv

	/* relocate ourselves to <tmp_reloc_org> */
	#  PhysBcopy(RELOC, tmp_reloc_org, WORK_AREA_SIZE);
	mov	$RELOC, %esi
	mov	$tmp_reloc_org, %edi
	mov	$WORK_AREA_SIZE, %ecx
	mov	%ds, %ax
	mov	%ax, %es
	rep
	movsb

	/* TBD - could probably also do this by munging a seg descriptor.
	 * would it be easier?
	 */
	ljmp	$KERN_CODE_SEG, tmp_reloc_org+$1f
1:
	/* now we are PIC - caveats */
	/* move the stack pointer to the new copy */
	add	tmp_reloc_org-xxx, %esp

	/* push some number of args onto the stack */
	pushl	$0			# nominally a cyl offset in the boot.
	pushl	0x8(%eax)		# argv[2] = bootdev
	pushl	0x4(%eax)		# argv[1] = howto
	pushl	$0			# dummy 'return' address
	
	/*  push on our entry address */
	mov	$0x08, %ebx		# segment
	pushl	%ebx
	pushl	%ecx

	/* copy loaded file to its destination (TBD) */
	#  PhysBcopy(kcopy, korg, ksize);
	mov	kern_copy_org, %esi
	mov	boot_area_org, %edi
	mov	xxxksize, %ecx
	mov	%ds, %ax
	mov	%ax, %es
	rep
	movsb	

	/* convert the PC (and code seg) */
	lret
#endif


/*
 * C library -- _setjmp, _longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(setjmp)
	movl	4(%esp),%ecx 
	movl	0(%esp),%edx
	movl	%edx, 0(%ecx)
	movl	%ebx, 4(%ecx)
	movl	%esp, 8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	%eax,24(%ecx)
	movl	$0,%eax
	ret

ENTRY(longjmp)
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	cmpl	$0,%eax
	jne	1f
	movl	$1,%eax
1:	movl	%ecx,0(%esp)
	ret
