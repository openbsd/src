/*	$OpenBSD: ldasm.S,v 1.32 2017/08/27 21:59:52 deraadt Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#define DL_DATA_SIZE	(16*4)
#include <sys/syscall.h>
#include <machine/asm.h>

	.text
	.align  16,0xcc
	.globl  _dl_start
	.type   _dl_start,@function
_dl_start:
	movl	%esp,%eax		# save stack pointer for _rtld
	subl	$DL_DATA_SIZE,%esp	# allocate dl_data
	call	1f			# push &_DYNAMIC...
1:	addl	$(_DYNAMIC-1b),(%esp)	# ...for dl_boot_bind
	movl	%esp,%ebx
	movl	%ebx,%edi		# save dl_data arg for dl_boot
	pushl	%ebx			# push dl_data for dl_boot_bind

	mov	%eax, %esi		# save stack for dl_boot

	pushl	%eax			# load saved SP for dl_boot_bind

	call	_dl_boot_bind@PLT	# _dl_boot_bind(sp,dl_data)

	pushl	%edi			# push saved dl_data
	movl	%edi,%ebp
	movl	(7*4)(%ebp),%eax
	pushl	%eax			# push loff from dl_data

	movl	%esi,%ebp
	movl	$4,%eax
	imull	0(%ebp),%eax
	addl	$8,%eax
	addl	%ebp,%eax
	push	%eax			# push envp

	leal	4(%ebp),%eax
	push	%eax			# push argv

	call	_dl_boot@PLT		# _dl_boot(argv,envp,loff,dl_data)

	addl	$7*4,%esp		# pop args

	addl	$DL_DATA_SIZE,%esp	# return dl_data

	call	1f			# %edx = cleanup
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	leal	_dl_dtors@GOTOFF(%ebx), %edx

	jmp	*%eax

	.align 16,0xcc
	.global _dl_bind_start
	.type _dl_bind_start,@function
_dl_bind_start:
	pushf				# save registers
	pushl   %eax
	pushl   %ecx
	pushl   %edx
	pushl   %ebx
	pushl   %ebp
	pushl   %esi
	pushl   %edi
	pushl   %ds
	pushl   %es

	pushl   44(%esp)		# Copy of reloff
	pushl   44(%esp)		# Copy of obj
	call    _dl_bind@PLT		# Call the binder
	addl    $8,%esp			# pop binder args
	movl    %eax,44(%esp)		# Store function to be called in obj

	popl    %es			# restore registers
	popl    %ds
	popl    %edi
	popl    %esi
	popl    %ebp
	popl    %ebx
	popl    %edx
	popl    %ecx
	popl    %eax
	popf

	leal    4(%esp),%esp		# Discard reloff, do not change eflags
	ret
