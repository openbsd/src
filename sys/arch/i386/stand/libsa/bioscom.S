/*	$OpenBSD: bioscom.S,v 1.3 1997/03/31 23:06:26 mickey Exp $	*/

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
 *	from: Mach, Revision 2.2  92/04/04  11:34:26  rpd
 *	$Id: bioscom.S,v 1.3 1997/03/31 23:06:26 mickey Exp $
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

/*
 * Serial bootblock interface routines
 * Copyright (c) 1994, J"org Wunsch
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */ 

	.file	"serial.s"

#include <machine/asm.h>

	.data
port:
	.long	0x2f8

	.text

/*
 * The serial port interface routines implement a simple polled i/o
 * interface to a standard serial port.  Due to the space restrictions
 * for the boot blocks, no BIOS support is used (since BIOS requires
 * expensive real/protected mode switches), instead the rudimentary
 * BIOS support is duplicated here.
 *
 * The base address for the i/o port is stored in 'port' variable upon
 * probing for device.  Console parameters are currently
 * hard-coded to 9600 Bd, 8 bit.  This can be changed in the
 * init_serial() function.
 */

/*
 * int com_probe();
 *	probe com[0-3] ports and store first found in 'port' variable
 */
ENTRY(com_probe)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx

	/*
	 * void init_serial(void)
	 * 	initialize the serial console port to 9600 Bd, 8 bpc
	 */
	movl	$port, %edx	# line control reg
	addb	5, %dl
	movl	$0x80, %eax
	outb	%al, %dx	# enable DLAB

	subb	$3, %dl		# divisor latch, low byte
	movb	$12, %al	# divisor 12: 9600 Bd
	outb	%al, %dx
	incl	%edx		# divisor latch, high byte
	movb	%ah, %al
	outb	%al, %dx

	addb	$2, %dl		# line control reg
	movb	$0x13, %al
	outb	%al, %dx	# 8 bit, no parity, 1 stop bit

	incl	%edx		# modem control reg
	movb	$3, %al
	outb	%al, %dx	# enable DTR/RTS

	/* now finally, flush the input buffer */
	incl	%edx		# line status reg
1:
	inb	%dx, %al
	testb	$0x01, %al
	jz	2f		# no more characters buffered
	subb	$5, %dl		# rx buffer reg
	inb	%dx, %al	# throw away
	addb	$5, %dl
	jmp	1b
2:
	popl	%edx
	popl	%ebp
	xorl	%eax, %eax	# is present (:
	incl	%eax
	ret

/*
 * void com_putc(ch) char ch;
 *	send ch to serial port
 *
 */

ENTRY(com_putc)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx

	movl	$port, %edx	# line status reg
	add	5, %edx
1:	inb	%dx, %al
	test	$0x20, %al
	jz	1b		# TX buffer not empty

	movb	8(%ebp), %al

	sub	$5, %edx	# TX output reg
	outb	%al, %dx	# send this one

	popl	%edx
	popl	%ebp
	ret

/*
 * int com_getc(void)
 *	read a character from serial port
 */

ENTRY(com_getc)
	pushl	%edx

	movl	$port, %edx	# line status reg
	addl	5, %edx
1:
	inb	%dx, %al
	testb	$0x01, %al
	jz	1b		# no RX char available

	xorl	%eax, %eax
	subl	$5, %edx	# RX buffer reg
	inb	%dx, %al	# fetch (first) character

	andb	$0x7F, %eax	# remove any parity bits we get
	cmpb	$0x7F, %eax	# make DEL...
	jne	2f
	movb	$0x08, %eax	# look like BS
2:
	popl	%edx
	ret

/*
 * int com_ischar(void)
 *       if there is a character pending, return true; otherwise return 0
 */
ENTRY(com_ischar)
	pushl	%edx

	xorl	%eax, %eax
	movl	$port, %edx	# line status reg
	addl	5, %edx
	inb	%dx, %al
	andb	$0x01, %al		# RX char available?

	pop	%edx
	ret

