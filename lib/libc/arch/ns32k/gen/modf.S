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
	.asciz "$OpenBSD: modf.S,v 1.2 1996/08/19 08:16:50 tholo Exp $"
#endif /* LIBC_SCCS */

ENTRY(modf)
	FRAME
	movl	8(fp),f0	/* value */
	movd	12(fp),r0	/* higher 32 bit of value */
	lshd	-20,r0		/* extract exponent */
	andd	0x7ff,r0	/* 11 lower bits */
	cmpd	r0,1023+30	/* compare if it's int part can fit in int */
	bgt	modf_overflow	/* nope else it's ok to truncld it to int*/
	truncld	f0,r0		/* get integer part */
	movdl	r0,f2		/* convert back to float */
	movl	f2,0(16(fp))	/* move integer part to *iptr */
	subl	f2,f0		/* return fract. part = value - *iptr */
	EMARF
	ret	0
modf_overflow:
	subd	1023+20,r0	/* bias 1023, and upper part of
				   floating point mantissa part */
	movqd	-1,r2		/* generate mask to get fract. part */
	cmpd	r0,32		/* if value > 2^52 (20+32) then all int part */
	bhi	modf_all_ip
	negd	r0,r0		/* shift right */
	lshd	r0,r2		/* here */
	comd	r2,r2		/* get fractional part complement mask */
	movd	8(fp),r1	/* get lower 32 bit of value */
	andd	r2,r1		/* mask fractional part off leave ip part */
	movd	r1,0(16(fp))	/* store ip part to *iptr */
	movd	12(fp),4(16(fp)) /* store other half to *iptr */
	subl	0(16(fp)),f0	/* return fract. part = value - *iptr */
	EMARF
	ret	0
modf_all_ip:
	movl	8(fp),0(16(fp))	/* copy value to *iptr */
	movdl	0,f0		/* return 0 for fract. part */
	EMARF
	ret	0
