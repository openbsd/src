/*	$OpenBSD: s_finite.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(finite)
#ifdef __i386__
	movl	8(%esp),%eax
	andl	$0x7ff00000, %eax
	cmpl	$0x7ff00000, %eax
	setne	%al
	andl	$0x000000ff, %eax
#else
	xorl	%eax,%eax
	movq	$0x7ff0000000000000,%rsi
	movq	%rsi,%rdi
	movsd	%xmm0,-8(%rsp)
	andq	-8(%rsp),%rsi
	cmpq	%rdi,%rsi
	setne	%al
#endif
	ret
