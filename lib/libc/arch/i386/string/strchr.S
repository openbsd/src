/*	$OpenBSD: strchr.S,v 1.6 2015/05/15 22:29:37 millert Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

WEAK_ALIAS(index, strchr)

ENTRY(strchr)
	movl	4(%esp),%eax
	movb	8(%esp),%cl
	.align 2,0x90
L1:
	movb	(%eax),%dl
	cmpb	%dl,%cl			/* found char??? */
	je 	L2
	incl	%eax
	testb	%dl,%dl			/* null terminator??? */
	jnz	L1
	xorl	%eax,%eax
L2:
	ret
