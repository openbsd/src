/*	$OpenBSD: strcmp.S,v 1.1 2004/08/11 10:29:59 pefo Exp $	*/

#include "DEFS.h"


/*
 * NOTE: this version assumes unsigned chars in order to be "8 bit clean".
 */
LEAF(strcmp)
	.set	noreorder
1:
	lbu	t0, 0(a0)		# get two bytes and compare them
	lbu	t1, 0(a1)
	beq	t0, zero, LessOrEq	# end of first string?
	nop
	bne	t0, t1, NotEq
	nop
	lbu	t0, 1(a0)		# unroll loop
	lbu	t1, 1(a1)
	beq	t0, zero, LessOrEq	# end of first string?
	addu	a0, a0, 2
	beq	t0, t1, 1b
	addu	a1, a1, 2
NotEq:
	j	ra
	 subu	v0, t0, t1
LessOrEq:
	j	ra
	 subu	v0, zero, t1
END(strcmp)
