/*	$OpenBSD: srt0.S,v 1.14 2003/11/22 09:18:25 grange Exp $	*/

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
#include <assym.h>

#define BOOTSTACK 0xfffc

	.globl	_C_LABEL(end)
	.globl	_C_LABEL(edata)
	.globl	_C_LABEL(boot)
	.globl	_C_LABEL(_rtt)
	.globl	_C_LABEL(bios_bootdev)
	.globl	_ASM_LABEL(pmm_init)
	.globl	Gdtr

	.text
	.code16
	.globl	_start
_start:
	popl %eax
	cmpl $BOOTMAGIC, %eax
	je	1f
#ifdef DEBUG
	movl	$0xb80a0, %ebx
	addr32 movl $0x07420742, (%ebx)
#endif
1:
	popl %edx
	cli
	pushl	%cs
	popl	%ds
	addr32 data32 lgdt	(Gdtr - LINKADDR)
	movl	%cr0, %eax
	orl $CR0_PE, %eax
	data32 movl %eax, %cr0
	data32 ljmp $8, $1f
1:
	.code32
	movl	$0x10,%eax
	mov	%ax,%ds
	mov	%ax,%ss
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	movl	$BOOTSTACK,%esp
	pushl	%edx
	movl	%edx, _C_LABEL(bios_bootdev)

	/* Now do it all */
	call	_ASM_LABEL(pmm_init)
#ifdef DEBUG
	movl	$0xb80a4, %ebx
	movl	$0x07520752, (%ebx)
#endif
	/* zero .bss */
	xorl	%eax, %eax
	movl	$_C_LABEL(end), %ecx
	subl	$_C_LABEL(edata),%ecx
	movl	$_C_LABEL(edata), %edi
	cld
	rep;	stosb

	call	_C_LABEL(boot)

	jmp	_C_LABEL(_rtt)

