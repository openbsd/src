/*	$OpenBSD: bioskbd.S,v 1.3 1997/03/31 23:06:28 mickey Exp $	*/

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
#if 0
#include <machine/console.h>
#include <machine/cpufunc.h>
#include "boot.h"

int
probe_keyboard(void)
{
	int i, retries = 5;
	unsigned char val;

	/* flush any noise in the buffer */
	while (inb(KB_STAT) & KB_BUF_FULL) {
		delay1ms();
		(void) inb(KB_DATA);
	}

	/* Try to reset keyboard hardware */
  again:
	while (--retries) {
#ifdef DEBUG
		printf("%d ", retries);
#endif
		while ((inb(KB_STAT) & KB_READY) == KB_READY) delay1ms();
		outb(KB_DATA, KB_RESET);
		for (i=0; i<1000; i++) {
			delay1ms();
			val = inb(KB_DATA);
			if (val == KB_ACK || val == KB_ECHO)
				goto gotack;
			if (val == KB_RESEND)
				break;
		}
	}
gotres:
#ifdef DEBUG
	printf("gotres\n");
#endif
	if (!retries) {
#ifdef DEBUG
		printf("gave up\n");
#endif
		return(1);
	}
gotack:
	delay1ms();
	while ((inb(KB_STAT) & KB_BUF_FULL) == 0) delay1ms();
	delay1ms();
#ifdef DEBUG
	printf("ACK ");
#endif
	val = inb(KB_DATA);
	if (val == KB_ACK)
		goto gotack;
	if (val == KB_RESEND)
		goto again;
	if (val != KB_RESET_DONE) {
#ifdef DEBUG
		printf("stray val %d\n", val);
#endif
		return(0);
	}
#ifdef DEBUG
	printf("ok\n");
#endif
	return(0);
}

#endif
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
	pushal

	movb	8(%ebp), %cl

	call	_C_LABEL(prot_to_real)

	movb	%cl, %al

	movb	$0x0e, %ah
	xorl	%bx, %bx
	incl	%bx
	int	$0x10		# display a byte

	data32
	call	_C_LABEL(real_to_prot)

	popal
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
	pushal

	call	_C_LABEL(prot_to_real)

	movb	$0x01, %ah
	int	$0x16
	setnz	%ah

	movb	%ah, %bl	# real_to_prot uses %eax

	data32
	call	_C_LABEL(real_to_prot)

	movzbl	%bl, %eax
	movl	%eax, 1f

	popal
	movl	1f, %eax
	ret
1:	.long	0

