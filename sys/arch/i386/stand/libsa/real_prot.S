/*	$OpenBSD: real_prot.S,v 1.5 1997/04/09 08:39:42 mickey Exp $	*/

/*
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
 *
 *	from: Mach, Revision 2.2  92/04/04  11:34:13  rpd
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
	.file "real_prot.S"

#include <machine/asm.h>
#include <machine/specialreg.h>

#define addr32  .byte 0x67
#define data32  .byte 0x66

	.data

	.globl	_codeseg
	.globl	_Gdtr
	.globl	_Idtr_prot
	.globl	_Idtr_real

	.text

/*
 *
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */

ENTRY(real_to_prot)
	/* guarantee that interrupts are disabled when in prot mode */
	cli
	addr32
	data32
	movl	%eax, 2f

	/* load the gdtr */
	addr32
	data32
	lgdt	_C_LABEL(Gdtr)

	/* set the PE bit of CR0 */
	movl	%cr0, %eax
	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	data32
	ljmp	$0x08, $1f

1:	/*
	 * we are in USE32 mode now
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	/* load idtr for debugger and BIOS iface */
	lidt	_Idtr_prot
	# movl	Leax, %eax
	.byte	0xb8
2:	.long	0	/* eax */
	ret

/*
 *
 * prot_to_real()
 *	transfer from protected mode to real mode
 * 
 */

ENTRY(prot_to_real)

	movl	%eax, 4f
	movl 	_codeseg, %eax
	movw	%ax, 2f

	/* Change to use16 mode. */
	ljmp	$0x18, $1f
1:
	/* clear the PE bit of CR0 */
	movl	%cr0, %eax
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0

	data32
	# ljmp	_codeseg, $2f
	.byte	0xea
	.long	3f
2:	.word	0
3:	
	/* setup: %ds, %es, %ss */
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	/* load idtr for BIOS operations */
	addr32
	data32
	lidt	_Idtr_real

	addr32
	data32
	# movl	Leax, %eax
	.byte	0xb8
4:	.long	0	/* %eax */

	sti
	data32
	ret

