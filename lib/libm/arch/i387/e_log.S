/*	$OpenBSD: e_log.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__ieee754_log)
	fldln2
	fldl	4(%esp)
	fyl2x
	ret
