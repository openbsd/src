/*	$OpenBSD: ffs.S,v 1.1 2004/08/11 10:29:59 pefo Exp $	*/

#include "DEFS.h"


/*
 * bit = ffs(value)
 */
LEAF(ffs)
	.set	noreorder
	beq	a0, zero, 2f
	move	v0, zero
1:
	and	v1, a0, 1		# bit set?
	addu	v0, v0, 1
	beq	v1, zero, 1b		# no, continue
	srl	a0, a0, 1
2:
	j	ra
	 nop
END(ffs)
