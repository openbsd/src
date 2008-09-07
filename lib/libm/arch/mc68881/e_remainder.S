/*	$OpenBSD: e_remainder.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(remainder)
	fmoved	sp@(4),fp0
	fremd	sp@(12),fp0
	fmoved	fp0,sp@-
	movel	sp@+,d0
	movel	sp@+,d1
	rts
