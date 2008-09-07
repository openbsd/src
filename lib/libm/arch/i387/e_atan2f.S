/*	$OpenBSD: e_atan2f.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(atan2f)
	flds	4(%esp)
	flds	8(%esp)
	fpatan
	ret
