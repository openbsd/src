/* 
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * HELSINKI UNIVERSITY OF TECHNOLOGY ALLOWS FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION. HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIMS ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: frexp.S,v 1.2 1996/08/19 08:16:47 tholo Exp $"
#endif /* LIBC_SCCS */

ENTRY(frexp)
	enter	[],8
	movd	12(fp),r1	/* value, high 32 bit */
	cmpd	0,r1		/* check if 0 */ 
	beq	frexp_zero	/* we do not support denormalized values */
	movd	r1,r2
	bicd	0x7ff00000,r2	/* clear exponent */
	ord	0x3fe00000,r2	/* set it to 1024 == exponent -11 == .5-1 */
	lshd	-20,r1		/* extract exponent */
	andd	0x7ff,r1	/* 11 lower bits */
	subd	1022,r1		/* unbias exponent */
	movd	r1,0(16(fp))
	movd	8(fp),-8(fp)	/* value, low 32 bit */
	movd	r2,-4(fp)	/* value without exponent, high 32 bit */
	movl	-8(fp),f0	/* load return value */
	exit	[]
	ret	0
frexp_zero:
	movqd	0,0(16(fp))	/* return exp = 0, mantissa = 9 */
	movdl	0,f0
	exit	[]
	ret	0

