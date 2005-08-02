/*	$OpenBSD: s_atan.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(atan)
	fldl	4(%esp)
	fld1
	fpatan
	ret
