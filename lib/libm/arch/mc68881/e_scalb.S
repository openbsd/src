/*	$OpenBSD: e_scalb.S,v 1.4 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(scalb)
	fmoved	%sp@(4),%fp0
	fbeq	Ldone
	fscaled	%sp@(12),%fp0
Ldone:
	rts
