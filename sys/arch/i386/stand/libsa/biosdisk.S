/*	$OpenBSD: biosdisk.S,v 1.6 1997/04/18 06:54:43 weingart Exp $	*/

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
#define	_LOCORE
#include "debug_md.h"
#include "biosdev.h"
#undef	_LOCORE

/*
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#	Call with	%ah = 0x2
#			%al = number of sectors
#			%ch = cylinder
#			%cl = sector
#			%dh = head
#			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#			%es:%bx = segment:offset of buffer
#	Return:		
#			%al = 0x0 on success; err code on failure
*/
ENTRY(biosread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	pushl	%ebx
	pushl	%edi
	pushl	%es

	movb	8(%ebp), %dl	# device
	movw	12(%ebp), %cx
	xchgb	%ch, %cl	# cylinder; the highest 2 bits of cyl is in %cl
	rorb	$2, %cl
	movb	16(%ebp), %dh	# head
	movb	20(%ebp), %al
	orb	%al, %cl
	incb	%cl		# sector; sec starts from 1, not 0
	movb	$0x2, %ah	# subfunction
	movb	24(%ebp), %al	# number of sectors
	movl	28(%ebp), %ebx	# offset
	movl	%ebx, %edi	# spilt off for seg:off pair
	andl	$0xf, %ebx	# atomic read for up to 64k
	shrl	$4, %edi

	BIOSINT(0x13)
	movb	$0, %al
	jnc	1f
	movb	%ah, %al
1:
	movzbl	%al, %eax	# return value in %ax

	popl	%es
	popl	%edi
	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret

ENTRY(bioswrite)

	movl	$1, %eax
	ret

/*
#
# biosdinfo():  return a word that represents the
#	max number of sectors and  heads and drives for this device
#
*/

ENTRY(biosdinfo)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx

	movb	8(%ebp), %dl		# diskinfo(drive #)
	movb	$0x08, %ah		# ask for disk info
	BIOSINT(0x13)
	movb	$1, %ah		# max head
	movb	$18, %al	# max sector
	jc	1f

	/* form a longword representing all this gunk */
	movb	%dh, %ah		# max head
	andb	$0x3f, %cl		# mask of cylinder gunk
	movb	%cl, %al		# max sector (and # sectors)
1:
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret


/*
#
# biosdreset(): reset disk system
#
*/

ENTRY(biosdreset)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx

	movb	$0x00, %ah		# ask for disk info
	BIOSINT(0x13)

	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

