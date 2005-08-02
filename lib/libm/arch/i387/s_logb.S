/*	$OpenBSD: s_logb.S,v 1.3 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(logb)
	fldl	4(%esp)
	fxtract
	fstp	%st
	ret
