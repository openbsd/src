/*	$OpenBSD: e_log.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(log)
	fldln2
	fldl	4(%esp)
	fyl2x
	ret
