/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_atan2.S,v 1.4 1995/05/08 23:46:28 jtc Exp $")

ENTRY(__ieee754_atan2)
	fldl	 4(%esp)
	fldl	12(%esp)
	fpatan
	ret
