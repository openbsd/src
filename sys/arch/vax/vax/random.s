/*	$NetBSD: random.s,v 1.2 1994/10/26 08:03:24 cgd Exp $	*/

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
 * This implementation Copyright (c) 1994 Ludd, University of Lule}, Sweden
 * All rights reserved.
 *
 * All bugs subject to removal without further notice.
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

	.data
	.globl	__randseed
__randseed:
	.long	1
	.text
	.globl _random
_random:
	.word 0x0
	movl	$16807,r0

	movl	__randseed,r1		# r2=16807*loword(__randseed)
	bicl3	$0xffff0000,r1,r2
	mull2	r0,r2
	ashl	$-16,r1,r1		# r1=16807*hiword(__randseed)
	bicl2	$0xffff0000,r1
	mull2	r0,r1
	bicl3	$0xffff0000,r2,r0
	ashl	$-16,r2,r2		# r1+=(r2>>16)
	bicl2	$0xffff0000,r2
	addl2	r2,r1
	ashl	$16,r1,r2		# r0|=r1<<16
	bisl2	r2,r0
	ashl	$-16,r1,r1		# r1=r1>>16

	ashl	$1,r1,r1
	movl	r0,r2
	rotl	$1,r0,r0
	bicl2	$0xfffffffe,r0
	bisl2	r0,r1
	movl	r2,r0
	bicl2	$0x80000000,r0
	addl2	r1,r0
	bgeq	L1
	subl2	$0x7fffffff,r0
L1:	movl	r0,__randseed
	ret
