/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_sqrt.S,v 1.6 2003/07/26 19:25:00 salo Exp $")

ENTRY(__ieee754_sqrt)
#ifdef __i386__
	fldl	4(%esp)
	fsqrt
#else
	sqrtsd	%xmm0,%xmm0
#endif
	ret
