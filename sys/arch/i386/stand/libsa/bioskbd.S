/*	$OpenBSD: bioskbd.S,v 1.2 1997/03/31 03:12:11 weingart Exp $	*/

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

/* 
 * int	kbd_probe();
 *	probe if keyboard present
 *
 */
ENTRY(kbd_probe)
	
	movl	1, %eax		/* present (: */
	ret

/*
# BIOS call "INT 10H Function 0Eh" to write character to console
#	Call with	%ah = 0x0e
#			%al = character
#			%bh = page
#			%bl = foreground color
*/
ENTRY(kbd_putc)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movb	8(%ebp), %cl

	call	_C_LABEL(prot_to_real)

	movb	%cl, %al

	movb	$0x0e, %ah
	xorl	%ebx, %ebx
	incl	%ebx
	int	$0x10		# display a byte

	data32
	call	_C_LABEL(real_to_prot)

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret


/*
# BIOS call "INT 16H Function 00H" to read character from the keyboard
#	Call with	%ah = 0x00
#	Return:		%ah = keyboard scan code
#			%al = ASCII character
*/
ENTRY(kbd_getc)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	call	_C_LABEL(prot_to_real)

	xorl	%eax, %eax
	int	$0x16

	movb	%al, %bl	# real_to_prot uses %eax

	data32
	call	_C_LABEL(real_to_prot)

	movzbl	%bl, %eax

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret


/*
# BIOS call "INT 16H Function 01H" to check whether a character is pending
#	Call with	%ah = 0x01
#	Return:	
#		If key waiting to be input:
#			%ah = keyboard scan code
#			%al = ASCII character
#			ZF  = clear
#		else
#			ZF  = set
*/
ENTRY(kbd_ischar)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	call	_C_LABEL(prot_to_real)

	movb	$0x01, %ah
	int	$0x16
	setnz	%ah

	movb	%ah, %bl	# real_to_prot uses %eax

	data32
	call	_C_LABEL(real_to_prot)

	movzbl	%bl, %eax

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
