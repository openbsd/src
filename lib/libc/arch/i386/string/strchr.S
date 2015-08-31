/*	$OpenBSD: strchr.S,v 1.7 2015/08/31 02:53:56 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "SYS.h"

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
END_STRONG(strchr)
