/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_scalbn.S,v 1.4 1995/05/09 00:19:06 jtc Exp $")

ENTRY(scalbn)
	fildl	12(%esp)
	fldl	4(%esp)
	fscale
	ret
