/*	$OpenBSD: random.c,v 1.1 2001/09/01 15:44:20 drahn Exp $	*/

/*
 * Copyright (c) 1990,1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Here is a very good random number generator.  This implementation is
 * based on ``Two Fast Implementations of the "Minimal Standard" Random
 * Number Generator'', David G. Carta, Communications of the ACM, Jan 1990,
 * Vol 33 No 1.  Do NOT modify this code unless you have a very thorough
 * understanding of the algorithm.  It's trickier than you think.  If
 * you do change it, make sure that its 10,000'th invocation returns
 * 1043618065.
 *
 * Here is easier-to-decipher pseudocode:
 *
 * p = (16807*seed)<30:0>	# e.g., the low 31 bits of the product
 * q = (16807*seed)<62:31>	# e.g., the high 31 bits starting at bit 32
 * if (p + q < 2^31)
 * 	seed = p + q
 * else
 *	seed = ((p + q) & (2^31 - 1)) + 1
 * return (seed);
 *
 * The result is in (0,2^31), e.g., it's always positive.
 */
#if 0
#include <machine/asm.h>

	.data
	.globl	_C_SYMBOL(_randseed)
_C_SYMBOL(_randseed):
	.long	1
	.text
ENTRY(random)
#	movl	#16807, d0
	lis	r5, 1
	ori	r5, r5, 0x6807
	lis	r4, _C_SYMBOL(_randseed)@h
	lwz	r6, _C_SYMBOL(_randseed)@l(r4)
#	mulsl	__randseed, d1:d0
	mulhw	r7, r6, r5
	mulhw	r8, r6, r5

#	lsll	#1, d0
#	roxll	#2, d1
#	addl	d1, d0
#	moveql	#1, d1
#	addxl	d1, d0
#	lsrl	#1, d0
	lis	r4, _C_SYMBOL(_randseed)@h
	stw	r6, _C_SYMBOL(_randseed)@l(r4)
#	movl	d0, __randseed
#	rts
#endif

extern int _randseed;
int
random()
{
	long long value;
	int p, q;
	value = 16807 * _randseed;
	p = value & (long long) (0xffffffff);
	q = (value >> 32) & (long long) (0xffffffff);
	if (((long long) p + q) < 0x3fffffff) {
		_randseed = p + q;
	} else {
		_randseed = (int)(((long long)p + q ) & 0x3ffffffe) +1;
	}
	return _randseed;
}
