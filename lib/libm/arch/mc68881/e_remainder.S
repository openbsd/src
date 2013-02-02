/*	$OpenBSD: e_remainder.S,v 1.4 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(remainder)
	fmoved	%sp@(4),%fp0
	fremd	%sp@(12),%fp0
	rts
