/*	$OpenBSD: e_fmod.S,v 1.4 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fmod)
	fldl	12(%esp)
	fldl	4(%esp)
1:	fprem
	fstsw	%ax
	sahf
	jp	1b
	fstp	%st(1)
	ret
