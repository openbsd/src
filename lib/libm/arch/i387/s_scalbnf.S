/*	$OpenBSD: s_scalbnf.S,v 1.4 2009/04/21 23:02:23 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

.global ldexpf
	ldexpf = scalbnf

ENTRY(scalbnf)
	fildl	8(%esp)
	flds	4(%esp)
	fscale
	fstp	%st(1)		/* bug fix for fp stack overflow */
	ret
