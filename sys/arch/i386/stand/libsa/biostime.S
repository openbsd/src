/*	$OpenBSD: biostime.S,v 1.10 1997/07/18 00:29:10 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
 * Copyright (c) 1997 Tobias Weingartner
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
 *	This product includes software developed by
 *	Michael Shalayeff and Tobias Weingartner.
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
#undef	_LOCORE

/*
 * int usleep(u_long us);
 *	sleep for that number of microseconds
 *	returns nonzero if failed
 * This BIOSes do not have this call:
 * <mho> SystemSOFT BIOS for VLSI Eagle  II Version 1.01 (2618-00)
 * <mho> BIOS Version: 1.00.04, Date: 08/08/96
 * <mho> (WHatever that means...)
 */
ENTRY(usleep)
	pushl	%ebx
	pushl	%ecx

	xorl	%edx, %edx
	movl	8(%esp), %ecx
	movw	%cx, %dx
	shrl	$16, %ecx

	movb	$0x86, %ah
	DOINT(0x15)
#ifdef notdef
	xorb	%al, %al
	jnc	1f
	movl	%ah, %al
1:	movzbl	%al, %eax
#endif
	popl	%ecx
	popl	%ebx
	ret


/*
 * int biostime(char buf[4])
 */
ENTRY(biostime)
	pushl %ebp
	movl %esp, %ebp

	pushl	%ecx
	pushl	%ebx

	movb	$0x02, %ah
	DOINT(0x1a)
	movb	$1, %al
	jc	1f

	/* Get address of buffer */
	movl	8(%ebp), %ebx
	movb	%ch, 0(%ebx)
	movb	%cl, 1(%ebx)
	movb	%dh, 2(%ebx)
	movb	%dl, 3(%ebx)
	xorl	%eax, %eax

1:	movzbl	%al, %eax

	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret


/*
 * int biosdate(char buf[4])
 */
ENTRY(biosdate)
	pushl %ebp
	movl %esp, %ebp

	pushl	%ecx
	pushl	%ebx

	movb	$0x04, %ah
	DOINT(0x1a)
	movb	$1, %al
	jc	1f

	/* Get address of buffer */
	movl	8(%ebp), %ebx
	movb	%ch, 0(%ebx)
	movb	%cl, 1(%ebx)
	movb	%dh, 2(%ebx)
	movb	%dl, 3(%ebx)
	xorl	%eax, %eax

1:	movzbl	%al, %eax

	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret

