/*	$OpenBSD: e_atan2.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__ieee754_atan2)
	fldl	 4(%esp)
	fldl	12(%esp)
	fpatan
	ret
