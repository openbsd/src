/*	$OpenBSD: bioscom.S,v 1.5 1997/07/17 22:48:42 mickey Exp $	*/

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

	.file	"bioscom.S"

#include <machine/asm.h>
#define	_LOCORE
#include <machine/biosvar.h>
#undef	_LOCORE

#define COM_ENT(i,op)		\
ENTRY(com/**/i/**/_/**/op)	; \
	xorl	%edx, %edx	; \
	movb	$i, %dl		; \
	jmp	1f
	
	.text
/*
 * int com<N>_probe();
 *	probe com<N> port
 */
COM_ENT(0,probe)
COM_ENT(1,probe)
COM_ENT(2,probe)
COM_ENT(3,probe)

1:	pushl	%ebx

	DOINT(0x11)		# get equipment (9-11 # of coms)
	shrl	$9, %eax
	andb	$7, %al
	cmpb	%al, %dl
	jl	1f
	xorl	%eax, %eax
	jmp	2f
1:
	xorl	%eax, %eax
	movb	$0xe2, %al
	DOINT(0x14)
	xorl	%eax, %eax
	incl	%eax
2:
	popl	%ebx
	ret

/*
 * void com<N>_putc(char ch)
 *	send ch to serial port
 *
 */

COM_ENT(0,putc)
COM_ENT(1,putc)
COM_ENT(2,putc)
COM_ENT(3,putc)

1:	pushl	%ebx
	movl	8(%esp), %eax
	movb	$1, %ah
	DOINT(0x14)
	xorl	%al, %al
	testb	$0x80, %ah
	jz	1f
	movb	%ah, %al
	andb	$0x7f, %al
1:	movzbl	%al, %eax
	popl	%ebx
	ret

/*
 * int com<N>_getc(void)
 *	read a character from serial port
 */

COM_ENT(0,getc)
COM_ENT(1,getc)
COM_ENT(2,getc)
COM_ENT(3,getc)

1:	pushl	%ebx
	xorl	%eax, %eax
	movb	$2, %ah
	DOINT(0x14)
	testb	$0x80, %ah
	jz	1f
	movl	$-1, %eax
	jmp	2f
1:	movzbl	%al, %eax
2:	popl	%ebx
	ret

/*
 * int com<N>_ischar(void)
 *       check for char available
 */

COM_ENT(0,ischar)
COM_ENT(1,ischar)
COM_ENT(2,ischar)
COM_ENT(3,ischar)

1:	pushl	%ebx
	xorl	%eax, %eax
	movb	$3, %al
	DOINT(0x14)
	movb	$0, %al
	testb	$1, %ah
	jz	1f
	incl	%eax
1:	movzbl	%al, %eax
	popl	%ebx
	ret

