/*	$OpenBSD: real_prot.S,v 1.2 1997/03/31 03:12:16 weingart Exp $	*/

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
 *	$Id: real_prot.S,v 1.2 1997/03/31 03:12:16 weingart Exp $
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
#ifndef NO_IDTR
	.globl	_Idtr_prot
	.globl	_Idtr_real
#endif

	.text

/*
 *
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */

.align 4,0x90
ENTRY(real_to_prot)
	/* guarantee that interrupt is disabled when in prot mode */
	cli

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

#ifndef NO_IDTR
	/* load idtr so we can debug */
	lidt	_Idtr_prot
#endif
	ret

/*
 *
 * prot_to_real()
 *	transfer from protected mode to real mode
 * 
 */

.align 4,0x90
ENTRY(prot_to_real)

	/*
	 * This is self-modifying code, but we need a writable
	 * code segment, and an intersegment return does not give us that.
	 */
	movw 	_codeseg, %ax
	pushw	%ax
	movw	$2f, %ax
	pushw	%ax

	/* Change to use16 mode. */
	ljmp	$0x18, $1f

1:	/* clear the PE bit of CR0 */
	movl	%cr0, %eax
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0

	/*
	 * make intersegment jmp to flush the processor pipeline
	 * using the fake stack frame set up earlier
	 * and reload CS register
	 */
#if 0
	hlt
#endif
	lret
2:	/*
	 * we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es
#ifndef	NO_IDTR
	/* load idtr so we can debug */
	addr32
	data32
	lidt	_Idtr_real
#endif
	sti
	data32
	ret

