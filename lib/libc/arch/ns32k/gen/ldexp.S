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
/*
 * HISTORY
 * 29-Apr-92  Tero Kivinen (kivinen) at Helsinki University of Technology
 *	Created.
 *
 *	$Id: ldexp.S,v 1.1.1.1 1995/10/18 08:41:37 deraadt Exp $
 */

/*
 * double ldexp (value, exp)
 * double value;
 * int exp;
 *
 * Ldexp returns value*2**exp, if that result is in range.
 * If underflow occurs, it returns zero.  If overflow occurs,
 * it returns a value of appropriate sign and largest
 * possible magnitude.  In case of either overflow or underflow,
 * errno is set to ERANGE.  Note that errno is not modified if
 * no error occurs.
 */

#include <machine/asm.h>

#define ERANGE 34

	.globl  _errno

ENTRY(ldexp)
	enter	[],8
	movd	12(fp),r1	/* get value, high 32 bit */
	lshd	-20,r1		/* extract exponent */
	andd	0x7ff,r1	/* 11 lower bits */
	subd	1023,r1		/* unbias exponent */
	movd	16(fp),r0	/* get exp */
	addd	r1,r0		/* add exponents */
	cmpd	r0,1023		/* most positive exponent */
	bgt	ldexp_overflow	/* too large */
	cmpd	r0,-1022	/* most negative exponent */
	blt	ldexp_underflow /* too small */
	addd	1023,r0		/* bias exponent */
	lshd	20,r0		/* convert exponent back to its place */
	movd	12(fp),r1	/* get value high 32 bit */
	bicd	0x7ff00000,r1	/* clear exponent */
	ord	r0,r1		/* insert exponent */
	movd	r1,-4(fp)	/* put high 32 bit to local variable */
	movd	8(fp),-8(fp)	/* value low 32 bit to local variable */
	movl	-8(fp),f0	/* return value * 2**exp */
	exit	[]
	ret	0		/* neither */
ldexp_underflow:
	movdl	0d0e0,f0	/* if underflow return 0, set errno */
	PIC_PROLOGUE
	movd	ERANGE,PIC_GOT(_errno)	/* set errno */
	PIC_EPILOGUE
	exit	[]
	ret	0
ldexp_overflow:
	movl	huge(pc),f0	/* if overflow return HUGE */
	movdl	0d0e0,f2
	cmpl	f2,8(fp)	/* check original sign */
	bgt	ldexp_positive
	negl	f0,f0		/* if negative, return -HUGE */
ldexp_positive:
	PIC_PROLOGUE
	movd	ERANGE,PIC_GOT(_errno)	/* set errno */
	PIC_EPILOGUE
	exit	[]
	ret	0
	
huge:	.long	0xffffffff	/* the largest number that can */
	.long	0x7fefffff	/* be represented in a long floating */
				/* number.  This is given in hex in order */
				/* to avoid floating conversions */
