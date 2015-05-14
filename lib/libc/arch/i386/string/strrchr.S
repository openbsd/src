/*	$OpenBSD: strrchr.S,v 1.5 2015/05/14 21:40:37 kettenis Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

STRONG_ALIAS(rindex, strrchr)

ENTRY(strrchr)
	pushl	%ebx
	movl	8(%esp),%edx
	movb	12(%esp),%cl
	xorl	%eax,%eax		/* init pointer to null */
	.align 2,0x90
L1:
	movb	(%edx),%bl
	cmpb	%bl,%cl
	jne	L2
	movl	%edx,%eax
L2:
	incl	%edx
	testb	%bl,%bl			/* null terminator??? */
	jnz	L1
	popl	%ebx
	ret
