/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_sqrt.S,v 1.4 1995/05/08 23:49:57 jtc Exp $")

ENTRY(__ieee754_sqrt)
	fldl	4(%esp)
	fsqrt
	ret
