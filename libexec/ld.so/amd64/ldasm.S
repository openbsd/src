/*	$OpenBSD: ldasm.S,v 1.21 2015/11/15 03:41:24 deraadt Exp $	*/

/*
 * Copyright (c) 2002,2004 Dale Rahn
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


#define DL_DATA_SIZE	(16*8)	 /* 16 * sizeof(ELF_Addr) */
#define DL_LOFF_OFFSET	(7*8)	/* index 7 */
#include <sys/syscall.h>
#include <machine/asm.h>

	.text
	.align	4
	.globl	_dl_start
	.type	_dl_start,@function
_dl_start:
	movq	%rsp, %r12		# save stack pointer for _rtld

	subq	$8, %rsp		# align stack
	andq	$~15, %rsp
	addq	$8, %rsp
	
	pushq	%rbx			# save ps_strings
	subq	$DL_DATA_SIZE, %rsp	# allocate dl_data

	leaq	_DYNAMIC(%rip),%rdx	# &_DYNAMIC
	movq	%rsp, %rsi		# dl_data for dl_boot_bind
	movq	%r12, %rdi		# load saved SP for dl_boot_bind
	call	_dl_boot_bind@PLT	# _dl_boot_bind(sp,dl_data,dynamicp)

	movq	%rsp, %rcx		# dl_data
	movq	DL_LOFF_OFFSET(%rsp), %rdx		# loff from dl_data

	movq	(%r12), %rdi
	leaq	16(%r12,%rdi,8), %rsi	# envp
	movq	%r12, %rdi
	addq	$8,%rdi			# argv
	call	_dl_boot@PLT		# _dl_boot(argv,envp,loff,dl_data)

	addq	$DL_DATA_SIZE,%rsp	# return dl_data

	leaq	_dl_dtors(%rip), %rdx	# %rdx = cleanup
	movq	%r12, %rsp
	jmp	*%rax

	.section	".text"

#define	DL_SYSCALL(n) DL_SYSCALL2(n,n)
#define	DL_SYSCALL2(n,c) \
	.global	__CONCAT(_dl_,n)		;\
	.type	__CONCAT(_dl_,n), @function	;\
	.align	4				;\
__CONCAT(_dl_,n):				;\
	movl	$(__CONCAT(SYS_,c)), %eax	;\
	movq	%rcx, %r10			;\
	syscall					;\
	jb	1f				;\
	ret

DL_SYSCALL(open)
DL_SYSCALL(fstat)
DL_SYSCALL(read)
DL_SYSCALL(write)
DL_SYSCALL(close)
DL_SYSCALL(issetugid)
DL_SYSCALL(getdents)
DL_SYSCALL(mprotect)
DL_SYSCALL(munmap)
DL_SYSCALL(gettimeofday)
DL_SYSCALL(exit)
DL_SYSCALL(readlink)
DL_SYSCALL(lstat)
DL_SYSCALL(utrace)
DL_SYSCALL(getentropy)
DL_SYSCALL(sendsyslog)
DL_SYSCALL(pledge)
DL_SYSCALL2(getcwd,__getcwd)
DL_SYSCALL2(set_tcb,__set_tcb)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL(sysctl)

1:
	/* error: result = -errno; - handled here. */
	neg	%rax
	ret


	.align 4
	.global _dl_bind_start
	.type _dl_bind_start,@function
_dl_bind_start:
	.cfi_startproc
	.cfi_adjust_cfa_offset	16
	pushfq				# save registers
	.cfi_adjust_cfa_offset	8
	/*.cfi_offset	%rflags, -16 */
	pushq	%rax
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%rax, -24
	pushq	%rcx
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%rcx, -32
	pushq	%rdx
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%rdx, -40
	pushq	%rsi
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%rsi, -48
	pushq	%rdi
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%rdi, -56
	pushq	%r8
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%r8, -64
	pushq	%r9
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%r9, -72
	pushq	%r10
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%r10, -80
	pushq	%r11
	.cfi_adjust_cfa_offset	8
	.cfi_offset	%r11, -88

	movq	80(%rsp), %rdi		# Copy of reloff
	movq	88(%rsp), %rsi		# Copy of obj
	call	_dl_bind@PLT		# Call the binder
	movq	%rax,88(%rsp)		# Store function to be called in obj

	popq	%r11			# restore registers
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%r11
	popq	%r10
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%r10
	popq	%r9
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%r9
	popq	%r8
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%r8
	popq	%rdi
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%rdi
	popq	%rsi
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%rsi
	popq	%rdx
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%rdx
	popq	%rcx
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%rcx
	popq	%rax
	.cfi_adjust_cfa_offset	-8
	.cfi_restore	%rax
	popfq
	.cfi_adjust_cfa_offset	-8
	/*.cfi_restore	%rflags */

	leaq	8(%rsp),%rsp		# Discard reloff, do not change eflags
	.cfi_adjust_cfa_offset	-8
	ret
	.cfi_endproc

