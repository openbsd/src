/*	$OpenBSD: s_copysignf.S,v 1.3 2016/09/12 19:47:02 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(copysignf)
	movl	8(%esp),%edx
	andl	$0x80000000,%edx
	movl	4(%esp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,4(%esp)
	flds	4(%esp)
	ret
END(copysignf)
