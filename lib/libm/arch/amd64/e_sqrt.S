/*	$OpenBSD: e_sqrt.S,v 1.4 2009/04/08 22:30:41 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(sqrt)
	sqrtsd	%xmm0,%xmm0
	ret
