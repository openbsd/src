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
