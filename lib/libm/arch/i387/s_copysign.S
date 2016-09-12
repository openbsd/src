/*	$OpenBSD: s_copysign.S,v 1.3 2016/09/12 19:47:02 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(copysign)
	movl	16(%esp),%edx
	andl	$0x80000000,%edx
	movl	8(%esp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,8(%esp)
	fldl	4(%esp)
	ret
END(copysign)
