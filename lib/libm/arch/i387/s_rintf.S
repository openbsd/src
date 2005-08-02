/*	$OpenBSD: s_rintf.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(rintf)
	flds	4(%esp)
	frndint
	ret
