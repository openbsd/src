/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_remainder.S,v 1.4 1995/05/08 23:49:37 jtc Exp $")

ENTRY(__ieee754_remainder)
	fldl	12(%esp)
	fldl	4(%esp)
1:	fprem1
	fstsw	%ax
	sahf
	jp	1b
	fstp	%st(1)
	ret
