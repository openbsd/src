/*	$OpenBSD: bioskbd.S,v 1.4 1997/04/09 08:39:23 mickey Exp $	*/

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
#include "biosdev.h"
#undef	_LOCORE

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
	pushl	%ebx

	movb	8(%esp), %al
	movb	$0x0e, %ah
	xorl	%ebx, %ebx
	incl	%ebx
	BIOSINT(0x10)		# display a byte

	popl	%ebx
	ret


/*
# BIOS call "INT 16H Function 00H" to read character from the keyboard
#	Call with	%ah = 0x00
#	Return:		%ah = keyboard scan code
#			%al = ASCII character
*/
ENTRY(kbd_getc)
	xorl	%eax, %eax
	BIOSINT(0x16)
	movzbl	%al, %eax
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
	movb	$0x01, %ah
	BIOSINT(0x16)
	setnz	%al
	movzbl	%al, %eax
	ret

