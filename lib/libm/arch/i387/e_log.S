/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_log.S,v 1.4 1995/05/08 23:48:39 jtc Exp $")

ENTRY(__ieee754_log)
	fldln2
	fldl	4(%esp)
	fyl2x
	ret
