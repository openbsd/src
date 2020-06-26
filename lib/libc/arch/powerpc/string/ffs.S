/*	$OpenBSD: ffs.S,v 1.1 2020/06/26 20:16:21 naddy Exp $ */
/*
 * Written by Christian Weisgerber <naddy@openbsd.org>.
 * Public domain.
 */
 
#include "SYS.h"

ENTRY(ffs)
	neg	%r4, %r3
	and	%r3, %r3, %r4
	cntlzw	%r3, %r3
	subfic	%r3, %r3, 32
	blr
END(ffs)
.protected
