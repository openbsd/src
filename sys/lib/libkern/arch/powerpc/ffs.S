/*	$OpenBSD: ffs.S,v 1.2 2020/10/27 01:26:53 gkoehler Exp $ */
/*
 * Written by Christian Weisgerber <naddy@openbsd.org>.
 * Public domain.
 */
 
#include <machine/asm.h>

ENTRY(ffs)
	RETGUARD_SETUP(ffs)
	neg	%r4, %r3
	and	%r3, %r3, %r4
	cntlzw	%r3, %r3
	subfic	%r3, %r3, 32
	RETGUARD_CHECK(ffs)
	blr
END(ffs)
