/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_sqrtf.S,v 1.4 2003/07/26 19:25:00 salo Exp $")

ENTRY(__ieee754_sqrtf)
#ifdef __i386__
	flds	4(%esp)
	fsqrt
#else
	sqrtss	%xmm0,%xmm0
#endif
	ret
