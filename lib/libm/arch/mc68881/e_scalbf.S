/*	$OpenBSD: e_scalbf.S,v 1.1 2011/05/28 21:55:04 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(scalbf)
	fmoves	sp@(4),fp0
	fbeq	Ldone
	fscales	sp@(8),fp0
Ldone:
	fmoves	fp0,sp@-
	movel	sp@+,d0
	rts
