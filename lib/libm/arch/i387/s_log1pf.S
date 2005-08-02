/*	$OpenBSD: s_log1pf.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * Since the fyl2xp1 instruction has such a limited range:
 *	-(1 - (sqrt(2) / 2)) <= x <= sqrt(2) - 1
 * it's not worth trying to use it.  
 */

ENTRY(log1pf)
	fldln2
	flds 4(%esp)
	fld1
	faddp
	fyl2x
	ret
