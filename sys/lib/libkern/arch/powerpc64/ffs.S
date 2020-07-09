/*	$OpenBSD: ffs.S,v 1.1 2020/06/10 20:19:29 naddy Exp $ */
/*
 * Written by Christian Weisgerber <naddy@openbsd.org>.
 * Public domain.
 */
 
#include <machine/asm.h>

ENTRY(ffs)
	neg	%r4, %r3
	and	%r3, %r3, %r4
	cntlzw	%r3, %r3
	subfic	%r3, %r3, 32
	blr
END(ffs)
