/*	$OpenBSD: debug_i386.S,v 1.8 1998/02/24 22:06:48 weingart Exp $	*/

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
#include <machine/trap.h>
#include <debug_md.h>
#undef _LOCORE

	.text
	.globl	alltraps

alltraps:
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
#ifdef DEBUG
	movl	$0xb8280, %edi
	movl	0x0c*4(%esp), %eax	# trapno
	addb	$'0', %al
	movb	$0x17, %ah
	shll	$16, %eax
	movb	0x0d*4(%esp), %al	# error
	addl	$'0', %al
	movb	$0x17, %ah
	movl	%ax, (%edi)
	hlt
#endif
	call	_check_regs
1:
#ifdef DEBUG
	movl	$0xb8290, %edi
	movl	$0x47394738, (%edi)
#endif
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popal
	addl	$0x8, %esp
	hlt
	iret

ENTRY(check_regs)
#ifdef DEBUG
	movl	$0xb8284, %edi
	movl	$0x47334732, (%edi)
#endif
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	$_reg, %edi
	cld
	movl	0x0c*4(%esp), %eax; stosl # %eax
	movl	0x0b*4(%esp), %eax; stosl # %ecx
	movl	0x0a*4(%esp), %eax; stosl # %edx
	movl	0x09*4(%esp), %eax; stosl # %ebx
	movl	0x08*4(%esp), %eax; stosl # %esp
	movl	0x07*4(%esp), %eax; stosl # %ebp
	movl	0x06*4(%esp), %eax; stosl # %esi
	movl	0x05*4(%esp), %eax; stosl # %edi
	movl	0x0f*4(%esp), %eax; stosl # %eip
	movl	0x11*4(%esp), %eax; stosl # %eflags
	movl	0x10*4(%esp), %eax; stosl # %cs
	movl		%ss,  %eax; stosl # %ss
	movl	0x04*4(%esp), %eax; stosl # %ds
	movl	0x03*4(%esp), %eax; stosl # %es
	movl	0x02*4(%esp), %eax; stosl # %fs
	movl	0x01*4(%esp), %eax; stosl # %gs

#ifdef DEBUG
	movl	$0xb8288, %edi
	movl	$0x47354734, (%edi)
#endif
	movl	0x0d*4(%esp), %eax	# trapno
	pushl	%eax
	call	_C_LABEL(dump_regs)
	popl	%eax

#ifdef DEBUG
	movl	$0xb828c, %edi
	movl	$0x47374736, (%edi)
#endif
	ret


