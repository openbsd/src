/*	$OpenBSD: uthread_machdep_asm.S,v 1.3 2004/02/25 04:10:53 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/asm.h>

/*
 * Switch stacks
 */

/* void _thread_machdep_switch(new, oldsave); */
ENTRY(_thread_machdep_switch)
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rbx

	pushq	%rbp
	pushq	%rsi
	pushq	%rdi

	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11

	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	$0xdeaf12345678beaf, %r15
	pushq	%r15

	movl	%ds,%eax
	pushq	%rax
	movl	%es,%eax
	pushq	%rax
	movl	%fs,%eax
	pushq	%rax
	movl	%gs,%eax
	pushq	%rax

	movq	%rsp, 0(%rsi)			/* *arg2 = %rsp */
	movq	0(%rdi), %rsp			/* %rsp = *arg1 */

	popq	%rax
	movl	%eax,%gs
	popq	%rax
	movl	%eax,%fs
	popq	%rax
	movl	%eax,%es
	popq	%rax
	movl	%eax,%ds

	popq	%r15 	# flag word.
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12

	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8

	popq	%rdi
	popq	%rsi
	popq	%rbp

	popq	%rbx
	popq	%rdx
	popq	%rcx
	popq	%rax
	ret
