/*	$OpenBSD: debug_i386.S,v 1.1 1997/04/01 04:52:46 mickey Exp $	*/

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
#include <machine/psl.h>
#define _LOCORE
#include <machine/segments.h>
#include <debug_md.h>
#undef _LOCORE

	.data
#ifndef NO_IDTR
	.align 3
	.globl	_Idtr_real
_Idtr_real:
	.word	1024
	.long	0

	.align 3
	.globl	_Idtr_prot
idt:
	.long	0, 0

_Idtr_prot:
	.word	. - idt
	.long	idt
#endif

	.text

ENTRY(check_regs)
	pushal		# 8 ones
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	pushfl
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	$_reg, %edi
	cld
	movl	0xc*4(%esp), %eax; stosl # %eax
	movl	0xb*4(%esp), %eax; stosl # %ecx
	movl	0xa*4(%esp), %eax; stosl # %edx
	movl	0x9*4(%esp), %eax; stosl # %ebx
	movl	0x8*4(%esp), %eax; stosl # %esp
	movl	0x7*4(%esp), %eax; stosl # %ebp
	movl	0x6*4(%esp), %eax; stosl # %esi
	movl	0x5*4(%esp), %eax; stosl # %edi
	movl	0xd*4(%esp), %eax; stosl # %eip
	movl	0x0*4(%esp), %eax; stosl # %eflags
	movl	0xe*4(%esp), %eax; stosl # %cs
	movl		%ss, %eax; stosl # %ss
	movl	0x4*4(%esp), %eax; stosl # %ds
	movl	0x3*4(%esp), %eax; stosl # %es
	movl	0x2*4(%esp), %eax; stosl # %fs
	movl	0x1*4(%esp), %eax; stosl # %gs

	call	_C_LABEL(dump_regs)

	popfl
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popal
	lret



