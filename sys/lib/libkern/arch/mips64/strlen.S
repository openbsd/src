/*	$OpenBSD: strlen.S,v 1.1 2004/08/11 10:29:59 pefo Exp $	*/

#include "DEFS.h"


/*
 * strlen(str)
 */
LEAF(strlen)
	.set	noreorder
	addu	v1, a0, 1
1:
	lb	v0, 0(a0)		# get byte from string
	addu	a0, a0, 1		# increment pointer
	bne	v0, zero, 1b		# continue if not end
	nop
	j	ra
	 subu	v0, a0, v1		# compute length - 1 for '\0' char
END(strlen)
