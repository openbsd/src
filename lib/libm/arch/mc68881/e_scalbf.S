/*	$OpenBSD: e_scalbf.S,v 1.2 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(scalbf)
	fmoves	%sp@(4),%fp0
	fbeq	Ldone
	fscales	%sp@(8),%fp0
Ldone:
	rts
