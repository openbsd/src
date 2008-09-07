/*	$OpenBSD: e_sqrt.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(sqrt)
#ifdef __i386__
	fldl	4(%esp)
	fsqrt
#else
	sqrtsd	%xmm0,%xmm0
#endif
	ret
