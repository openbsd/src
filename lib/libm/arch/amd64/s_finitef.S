/*	$OpenBSD: s_finitef.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(finitef)
#ifdef __i386__
	movl	4(%esp),%eax
	andl	$0x7f800000, %eax
	cmpl	$0x7f800000, %eax
	setne	%al
	andl	$0x000000ff, %eax
#else
	xorl	%eax,%eax
	movl	$0x7ff00000,%esi
	movss	%xmm0,-4(%rsp)
	andl	-4(%rsp),%esi
	cmpl	$0x7ff00000,%esi
	setne	%al
#endif
	ret
