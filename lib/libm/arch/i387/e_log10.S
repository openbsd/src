/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_log10.S,v 1.4 1995/05/08 23:49:24 jtc Exp $")

ENTRY(__ieee754_log10)
	fldlg2
	fldl	4(%esp)
	fyl2x
	ret
