/*	$OpenBSD: s_significand.S,v 1.4 2016/09/12 19:47:02 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(significand)
	fldl	4(%esp)
	fxtract
	fstp	%st(1)
	ret
END(significand)
