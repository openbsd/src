/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_atan.S,v 1.4 1995/05/08 23:50:41 jtc Exp $")

ENTRY(atan)
	fldl	4(%esp)
	fld1
	fpatan
	ret
