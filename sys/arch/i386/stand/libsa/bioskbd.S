/*	$OpenBSD: bioskbd.S,v 1.8 1997/07/18 00:29:09 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#define	_LOCORE
#include <machine/biosvar.h>
#include <dev/isa/isareg.h>
#include <dev/ic/mc146818reg.h>
#include <i386/isa/nvram.h>
#undef	_LOCORE

/* 
 * int	kbd_probe();
 *	probe if keyboard present
 *
 */
ENTRY(kbd_probe)
	movb	$NVRAM_EQUIPMENT, %al
	outb	%al, $IO_RTC
	inb	$(IO_RTC+1), %al
	xorl	%edx, %edx
	andb	$(NVRAM_EQUIPMENT_KBD|NVRAM_EQUIPMENT_DISPLAY), %al
	cmpb	$(NVRAM_EQUIPMENT_KBD|NVRAM_EQUIPMENT_DISPLAY), %al
	je	1f
	incl	%edx
1:	movl	%edx, %eax
	ret

/*
 * void kbd_putc(int c)
 *	output byte to console
 */
ENTRY(kbd_putc)
	pushl	%ebx

	xorl	%eax, %eax
	movb	8(%esp), %al
	movb	$0x0e, %ah
	xorl	%ebx, %ebx
	incl	%ebx
	DOINT(0x10)		# display a byte
	popl	%ebx
	ret

/*
 * int kbd_getc(void)
 *	read character from keyboard
 */
ENTRY(kbd_getc)
	pushl	%ebx
	pushl	%ecx
	xorl	%eax, %eax
	DOINT(0x16)
	movzbl	%al, %eax
	popl	%ecx
	popl	%ebx
	ret

/*
 * int kbd_ischar(void)
 *	check if there is character available from keyboard
 */
ENTRY(kbd_ischar)
	pushl	%ebx
	pushl	%ecx
	xorl	%eax, %eax
	incb	%ah
	DOINT(0x16)
	setnz	%al
	movzbl	%al, %eax
	popl	%ecx
	popl	%ebx
	ret

