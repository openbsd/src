/*	$OpenBSD: ldasm.S,v 1.4 2004/05/25 15:56:18 deraadt Exp $	*/

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
	pushq	%rbx			# save ps_strings
	subq	$DL_DATA_SIZE, %rsp	# allocate dl_data

	leaq	_DYNAMIC(%rip),%rdx	# &_DYNAMIC
	movq	%rsp, %rsi		# dl_data for dl_boot_bind
	movq	%r12, %rdi		# load saved SP for dl_boot_bind
	call	_dl_boot_bind@PLT	# _dl_boot_bind(sp,dl_data,dynamicp)

	movq	%rsi, %rcx		# dl_data
	movq	DL_LOFF_OFFSET(%rsi), %rdx		# loff from dl_data

	movq	(%r12), %rdi
	leaq	16(%r12,%rdi,8), %rsi	# envp
	movq	%r12, %rdi
	addq	$8,%rdi			# argv
	call	_dl_boot@PLT		# _dl_boot(argv,envp,loff,dl_data)

	addq	$DL_DATA_SIZE,%rsp	# return dl_data

#	popq	%rbx			# %rbx = ps_strings - XXXDSR
#	popq	%rdx			# %rdx = cleanup - XXXDSR
#	popq	%rcx			# %rcx = obj_main - XXXDSR
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
DL_SYSCALL(fcntl)
DL_SYSCALL(fstat)
DL_SYSCALL(stat)
DL_SYSCALL(read)
DL_SYSCALL(write)
DL_SYSCALL(close)
DL_SYSCALL(issetugid)
DL_SYSCALL(getdirentries)
DL_SYSCALL(mprotect)
DL_SYSCALL(munmap)
DL_SYSCALL(exit)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL2(sysctl,__sysctl)

1:
	/* error: result = -errno; - handled here. */
	neg	%rax
	ret


	/* _dl_sigprocmask: does not handle NULL new set */

	.align 4
	.global _dl_sigprocmask
	.type _dl_sigprocmask,@function
_dl_sigprocmask:
	movq	(%rsi),%rsi		# fetch indirect...
	movl	$SYS_sigprocmask, %eax
	movq	%rcx, %r10
	syscall
	jc	1b		 /* error: result = -errno */
	testq	%rdx,%rdx		# test if old mask requested
	jz	2f
	movq	%rax,(%rdx)		# store old mask
	xorq	%rax,%rax
2:	ret

	.align 4
	.global _dl_bind_start
	.type _dl_bind_start,@function
_dl_bind_start:
	pushfq				# save registers
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11

	movq	80(%rsp), %rdi		# Copy of reloff
	movq	88(%rsp), %rsi		# Copy of obj
	call	_dl_bind@PLT		# Call the binder
	movq	%rax,88(%rsp)		# Store function to be called in obj

	popq	%r11			# restore registers
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rax
	popfq

	leaq	8(%rsp),%rsp		# Discard reloff, do not change eflags
	ret

