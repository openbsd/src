/*	$OpenBSD: srt0.S,v 1.8 1997/05/29 05:32:06 mickey Exp $	*/
/*	$NetBSD: srt0.c,v 1.3 1994/10/27 04:21:59 cgd Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)srt0.c	5.3 (Berkeley) 4/28/91
 */

#include <machine/asm.h>
#include <machine/psl.h>
#define _LOCORE
#include <machine/segments.h>
#undef _LOCORE

	.globl	_end
	.globl	_edata
	.globl	_boot
	.globl	__rtt

	.text
	.globl	start_boot
start_boot:
#ifdef DEBUG
	movl	$0xb80a0, %ebx
	movl	$0x07420742, (%ebx)
#endif
	call	pmm_init	/* init protected moded memory management */

#ifdef DEBUG
	movl	$0xb80a4, %ebx
	movl	$0x07520752, (%ebx)
#endif

	popl	%eax
	cmpl	$BOOTMAGIC, %eax
	je	1f
#ifdef DEBUG
	movl	$0xb80a8, %ebx
	movl	$0x4741474d, (%ebx)
#endif
1:
	/* zero .bss */
	xorl	%eax, %eax
	movl	$_end, %ecx
	subl	$_edata,%ecx
	movl	$_edata, %edi
	cld
	rep;	stosb

	call	_boot

	.data
	.align 3
_Idtr_reset:
	.word	0
	.long	0

	.text
__rtt:
#ifdef DEBUG
	movl	$0xb8000, %ebx
	movl	$0x4f514f51, (%ebx)
#endif
	movw	$0x1234, %ax
	movw	%ax, 0x472	# warm boot

	/* Try to use the KBD to reboot system */
	movl	$0xfe, %eax
	movl	$0x64, %edx
	outb	%al, %dx
	movl	$1, %eax
	pushl	%eax
	call	_C_LABEL(usleep)
	popl	%eax

	movl	$0xfe, %eax
	movl	$0x64, %edx
	outb	%al, %dx
	movl	$1, %eax
	pushl	%eax
	call	_C_LABEL(usleep)
	popl	%eax

#ifdef DEBUG
	movl	$0xb8000, %ebx
	movl	$0x07310731, (%ebx)
#endif

	/* Try to cause a tripple fault... */
	lidt    _Idtr_reset
	xorl	%eax, %eax
	divl	%eax, %eax

	/* Again... */
	int $0x8

	/* Again... */
	movl	$0, %esp	# segment violation
	ret

