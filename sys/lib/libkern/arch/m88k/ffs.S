/*	$OpenBSD: ffs.S,v 1.1 2004/01/21 14:28:36 miod Exp $	*/

/*
 * Written by Miodrag Vallat, 2004 - Public Domain
 */

#include <machine/asm.h>

ENTRY(ffs)
	bcnd	eq0, r2, 1f	! ff1(0) = 32, treat differently
	ff1	r3, r2
	jmp.n	r1
	 add	r2, r3, 1	! ff1 returns 0-31, need to increment
1:
	jmp.n	r1
	 or	r2, r0, r0
