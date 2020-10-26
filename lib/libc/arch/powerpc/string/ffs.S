/*	$OpenBSD: ffs.S,v 1.2 2020/10/26 22:07:05 gkoehler Exp $ */
/*
 * Written by Christian Weisgerber <naddy@openbsd.org>.
 * Public domain.
 */
 
#include "SYS.h"

ENTRY(ffs)
	RETGUARD_SETUP(ffs)
	neg	%r4, %r3
	and	%r3, %r3, %r4
	cntlzw	%r3, %r3
	subfic	%r3, %r3, 32
	RETGUARD_CHECK(ffs)
	blr
END(ffs)
.protected
