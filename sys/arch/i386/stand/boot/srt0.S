/*	$OpenBSD: srt0.S,v 1.10 1998/05/14 20:59:12 mickey Exp $	*/

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

	.globl	_C_LABEL(end)
	.globl	_C_LABEL(edata)
	.globl	_C_LABEL(boot)
	.globl	_C_LABEL(_rtt)
	.globl	_ASM_LABEL(pmm_init)

	.text
	.globl	start
start:
#ifdef DEBUG
	movl	$0xb80a0, %ebx
	movl	$0x07420742, (%ebx)
#endif
	call	_ASM_LABEL(pmm_init)
#ifdef DEBUG
	movl	$0xb80a4, %ebx
	movl	$0x07520752, (%ebx)
#endif
	popl	%eax
	cmpl	$BOOTMAGIC, %eax
	je	1f
#ifdef DEBUG
	movl	$0xb80a8, %ebx
	movl	$0xcf41cf4d, (%ebx)
#endif
1:
	/* zero .bss */
	xorl	%eax, %eax
	movl	$_C_LABEL(end), %ecx
	subl	$_C_LABEL(edata),%ecx
	movl	$_C_LABEL(edata), %edi
	cld
	rep;	stosb

	call	_C_LABEL(boot)

	jmp	_C_LABEL(_rtt)

