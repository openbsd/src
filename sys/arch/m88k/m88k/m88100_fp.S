/* $OpenBSD: m88100_fp.S,v 1.1 2004/04/29 14:33:27 miod Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* Floating point trouble routines */
#include "assym.h"
#include <machine/trap.h>
#include <machine/asm.h>

#define destsize 10

/* Floating-Point Status Register bits */
#define inexact 0
#define overflow 1
#define underflow 2
#define divzero 3
#define oper 4

#define sign 31
#define s1size 9
#define s2size 7
#define dsize 5

#define FADDop 0x05
#define FSUBop 0x06
#define FCMPop 0x07
#define FMULop 0x00
#define FDIVop 0x0e
#define FSQRTop 0x0f
#define INTop 0x09
#define NINTop 0x0a
#define TRNCop 0x0b

#define s1nan 7
#define s2nan 6
#define s1inf 5
#define s2inf 4
#define s1zero 3
#define s2zero 2
#define sigbit 19

#define modehi 30
#define modelo 29
#define rndhi 15
#define rndlo 14
#define efunf 7
#define efovf 6
#define efinx 5

ASENTRY(m88100_Xfp_precise)
	or	r29, r3,  r0		/* r29 is now the E.F. */
	subu	r31, r31, 40
	st	r1,  r31, 32
	st	r29, r31, 36

	ld	r2, r29, EF_FPSR * 4
	ld	r3, r29, EF_FPCR * 4
	ld	r4, r29, EF_FPECR * 4
	ld	r5, r29, EF_FPHS1 * 4
	ld	r6, r29, EF_FPLS1 * 4
	ld	r7, r29, EF_FPHS2 * 4
	ld	r8, r29, EF_FPLS2 * 4
	ld	r9, r29, EF_FPPT * 4


	/*
	 * Load into r1 the return address for the 0 handlers. Looking at
	 * FPECR, branch to the appropriate 0 handler. However, if none of the
	 * 0 bits are enabled, then a floating point instruction was issued
	 * with the floating point unit disabled. This will cause an
	 * unimplemented opcode 0.
	 */

	or.u	r1,r0,hi16(wrapup)	/* load return address of function */
	or	r1,r1,lo16(wrapup)

	bb0	6,r4, 3f		/* branch to FPunimp if bit set */
	br	FPuimp
3:
	bb0	7,r4, 4f		/* branch to FPintover if bit set */
	br	FPintover
4:
#if 0
	bb0	5,r4, 5f		/* branch to FPpriviol if bit set */
	br	FPpriviol
#endif
5:
	bb0	4,r4, 6f		/* branch to FPresoper if bit set */
	br	FPresoper
6:
	bb0	3,r4, 7f		/* branch to FPdivzero if bit set */
	br	FPdivzero
7:
	or.u	r4, r4, 0xffff

ASLOCAL(FPuimp)
	subu	r31,r31,40		/* allocate stack */
	st	r1,r31,36		/* save return address */
	st	r3,r31,32		/* save exception frame */
	or	r2,r0,T_FPEPFLT		/* load trap type */
	or	r3, r29, r0
	bsr	_C_LABEL(m88100_trap)
	ld	r1,r31,36		/* recover return address */
	addu	r31,r31,40		/* deallocate stack */
	jmp	r1

	/*
	 * To write back the results to the user registers, disable exceptions
	 * and the floating point unit. Write FPSR and FPCR and load the SNIP
	 * and SFIP.
	 * r5 will contain the upper word of the result
	 * r6 will contain the lower word of the result
	 */

ASLOCAL(wrapup)
	tb1	0,r0,0		/* make sure all floating point operations */
				/* have finished */
	ldcr	r10, cr1	/* load the PSR */
#if 0
	set	r10, r10, 1<PSR_FPU_DISABLE_BIT>
#endif
	set	r10, r10, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10, cr1

	ld	r1, r31, 32
	ld	r29, r31, 36
	addu	r31, r31, 40

	fstcr	r2, FPSR	/* write revised value of FPSR */
	fstcr	r3, FPCR	/* write revised value of FPCR */

	/* result writeback routine */
	addu	r3, r29, EF_R0 * 4
	extu	r2, r9, 5<0>		/* get 5 bits of destination register */
	bb0	5, r9, writesingle	/* branch if destination is single */

/* writedouble here */
	st	r5, r3 [r2]		/* write high word */
	add	r2, r2, 1		/* for double, the low word is the */
	/* unspecified register */
	clr	r2, r2, 27<5>		/* perform equivalent of mod 32 */
ASLOCAL(writesingle)
	st	r6, r3 [r2]		/* write low word into memory */
	jmp	r1

/*
 * Check if the numerator is zero. If the numerator is zero, then handle
 * this instruction as you would a 0/0 invalid operation.
 */

ASLOCAL(FPdivzero)
	st	r1,r31,0		/* save return address */
	bb1	s1size,r9,1f		/* branch if numerator double */
/* single number */
	clr	r10,r5,1<sign>	/* clear sign bit */
	extu	r11,r6,3<29>	/* grab upper bits of lower word */
	or	r10,r10,r11	/* combine ones of mantissa */
	bcnd	eq0,r10,resoper	/* numerator is zero, handle reserved operand */
	br	setbit		/* set divzero bit */
1:
/* double number */
	clr	r10,r5,1<sign>	/* clear sign bit */
	or	r10,r10,r6	/* or high and low words */
	bcnd	ne0,r10,setbit	/* set divzero bit */

/*
 * The numerator is zero, so handle the invalid operation by setting the
 * invalid operation bit and branching to the user handler if there is one
 * or writing a quiet NaN to the destination.
 */

ASLOCAL(resoper)
	set	r2,r2,1<oper>
#ifdef HANDLER
	bb0	oper,r3,noreshand	/* branch to execute default handling */
					/* for reserved operands */
	bsr	_handler		/* branch to user handler */
	br	FP_div_return
#endif

noreshand:
	set	r5,r0,0<0>	/* put a NaN in high word */
	set	r6,r0,0<0>	/* put a NaN in low word */
	br	FP_div_return
				/* writing to a word which may be ignored */
				/* is just as quick as checking the precision */
				/* of the destination */

/*
 * The operation is divide by zero, so set the divide by zero bit in the
 * FPSR. If the user handler is set, then go to the user handler, else
 * go to the default mode.
 */

setbit:
	set	r2,r2,1<divzero>
#ifdef HANDLER
	bb0	divzero,r3,default	/* go to default routine if no hdlr */
	bsr	_handler		/* execute handler routine */
	br	FP_div_return
#endif

/*
 * Considering the sign of the numerator and zero, write a correctly
 * signed infinity of the proper precision into the destination.
 */

default:
	bb1	dsize,r9,FPzero_double	/* branch to handle double result */
FPzero_single:
	clr	r10,r5,31<0>	/* clear all of S1HI except sign bit */
	xor	r10,r7,r10	/* xor the sign bits of the operands */
	or.u	r6,r0,0x7f80	/* load single precision infinity */
	br.n	FP_div_return
	 or	r6,r6,r10	/* load correctly signed infinity */

FPzero_double:
	clr	r10,r5,31<0>	/* clear all of S1HI except sign bit */
	xor	r10,r7,r10	/* xor the sign bits of the operands */
	or.u	r5,r0,0x7ff0	/* load double precision infinity */
	or	r5,r5,r10	/* load correctly signed infinity */
	or	r6,r0,r0	/* clear lower word of double */

FP_div_return:
	ld	r1,r31,0	/* load return address */
	jmp	r1

/*
 * Both NINT and TRNC require a certain rounding mode, so check which
 * instruction caused the integer conversion overflow. Use a substitute
 * FPCR in r1, and modify the rounding mode if the instruction is NINT
 * or TRNC.
 */
ASLOCAL(FPintover)
	extu	r10,r9,5<11>		/* extract opcode */
	cmp	r11,r10,INTop		/* see if instruction is INT */
	st	r1,r31,0		/* save return address */
	bb1.n	eq,r11,checksize	/* instruction is INT, do not modify */
					/* rounding mode */
	 or	r1,r0,r3		/* load FPCR into r1 */
	cmp	r11,r10,NINTop		/* see if instruction is NINT */
	bb1	eq,r11,NINT		/* instruction is NINT */
TRNC:
	clr	r1,r1,2<rndlo>		/* clear rounding mode bits, */
					/* instruction is TRNC */
	br.n	checksize		/* branch to check size */
	 set	r1,r1,1<rndlo>		/* make rounding mode round towards */
					/* zero */
NINT:
	clr	r1,r1,2<rndlo>		/* make rounding mode round to */
					/* nearest */

/* See whether the source is single or double precision. */

checksize:
	bb1	s2size,r9,checkdoub 	/* S2 is double, branch to see if */
					/* there is a false alarm */

/*
 * An integer has more bits than the mantissa of a single precision floating
 * point number, so to check for false alarms (i.e. valid conversion), simply
 * check the exponents. False alarms are detected for 2**30 to (2**30) - 1
 * and -2**30 to -2**31. Only seven bits need to be looked at since an
 * exception will not occur for the other half of the numbering system.
 * To speed up the processing, first check to see if the exponent is 32 or
 * greater.
 *
 * This code was originally written for the exponent in the control
 * register to have the most significant bit (8 - single, 11 - double)
 * flipped and sign extended. For precise exceptions, however, the most
 * significant bit is only sign extended. Therefore, the code was chopped
 * up so that it would work for positive values of real exponent which were
 * only sign extended.
 */

checksing:
	extu	r10,r7,7<20>	/* internal representation for single */
				/* precision is IEEE 8 bits sign extended */
				/* to 11 bits; for real exp. = 30, the */
				/* above instruction gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,31	/* compare to 32,but exp. off by 1 */
				/* these 2 instructions to speed up valid */
				/* execution of valid cases */
	bb1	ge,r11,overflw	/* valid case, perform overflow routine */
	bb1	sign,r7,checksingn /* source operand is negative */

/*
 * If the number is positve and the exponent is greater than 30, than it is
 * overflow.
 */
checksingp:
	cmp	r10,r10,29	/* compare to 30, but exp. off by 1 */
	bb1	gt,r10,overflw	/* no false alarm, its overflow */
	br	conversionsp	/* finish single precision conversion */

/*
 * If the number is negative, and the exponent is 30, or 31 with a mantissa
 * of 0, then it is a false alarm.
 */
checksingn:
	cmp	r11,r10,30		/* compare to 31,but exp. off by 1 */
	bb1	lt,r11,conversionsn	/* exp. less than 31, so convert */
	extu	r10,r8,3<29>		/* get upper three bits of lower */
					/* mantissa */
	mak	r12,r7,20<3>		/* get upper 20 bits of mantissa */
	or	r10,r10,r12		/* form complete mantissa */
	bcnd	eq0,r10,conversionsn	/* complete conversion if mantissa */
					/* is 0 */
	br	overflw			/* no false alarm, its overflow */

/*
 * False alarms are detected for 2**30 to (2**30) - 1 and -2**30 to -2**31.
 * Only seven bits need to be looked at since an exception will not occur
 * for the other half of the numbering system.
 * To speed up the processing, first check to see if the exponent is 32 or
 * greater. Since there are more mantissa bits than integer bits, rounding
 * could cause overflow. (2**31) - 1 needs to be checked so that it does
 * not round to 2**31, and -2**31 needs to be checked in case it rounds to
 * -((2**31) + 1).
 */
checkdoub:
	extu	r10,r7,10<20>	/* internal representation for double */
				/* precision is the same IEEE 11 bits */
				/* for real exp. = 30, the */
				/* above instruction gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,31	/* compare to 32,but exp. off by 1 */
				/* these 2 instructions to speed up valid */
				/* execution of valid cases */
	bb1	ge,r11,overflw	/* valid case, perform overflow routine */
	bb1	sign,r7,checkdoubn /* source operand is negative */

/*
 * If the exponent is not 31, then the floating point number will be rounded
 * before the conversion is done. A branch table is set up with bits 4 and 3
 * being the rounding mode, and bits 2, 1, and 0 are the guard, round, and
 * sticky bits.
 */
checkdoubp:
	cmp	r11,r10,30	/* compare to 31, but exponent off by 1 */
	bb1	eq,r11,overflw	/* no false alarm, its overflow */
	extu	r12,r8,1<22>	/* get LSB for integer with exp. = 30 */
	mak	r12,r12,1<2>	/* start to set up field for branch table */
	extu	r11,r8,1<21>	/* get guard bit */
	mak	r11,r11,1<1>	/* set up field for branch table */
	or	r12,r11,r12	/* set up field for branch table */
	extu	r11,r8,21<0>	/* get bits for sticky bit */
	bcnd	eq0,r11,nostickyp /* do not set sticky */
	set	r12,r12,1<0>	/* set sticky bit */
nostickyp:
	rot	r11,r1,0<rndlo>		/* shift rounding mode to 2 LSB''s */
	mak	r11,r11,2<3>		/* set up field, clear other bits */
	or	r12,r11,r12		/* set up field for branch table */
	lda	r12,r0[r12]		/* scale r12 */
	or.u	r12,r12,hi16(ptable)	/* load pointer into table */
	addu	r12,r12,lo16(ptable)
	jmp	r12

ptable:
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	paddone
	br	conversiondp
	br	conversiondp
	br	paddone
	br	paddone
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	conversiondp
	br	paddone
	br	paddone
	br	paddone
	br	conversiondp
	br	paddone
	br	paddone
	br	paddone

/*
 * Add one to the bit of the mantissa which corresponds to the LSB of an
 * integer. If the mantissa overflows, then there is a valid integer
 * overflow conversion; otherwise, the mantissa can be converted to the
 * integer.
 */
paddone:
	or	r10,r0,r0	/* clear r10 */
	set	r10,r10,1<22>	/* set LSB bit to 1 for adding */
	addu.co	r8,r8,r10	/* add the 1 obtained from rounding */
	clr	r11,r7,12<20>	/* clear exponent and sign */
	addu.ci	r11,r0,r11	/* add carry */
	bb1	20,r11,overflw	/* overflow to 2**31, abort the rest */
	br.n	conversiondp	/* since the exp. was 30, and the exp. */
				/* did not round up to 31, the largest */
				/* number that S2 could become is 2**31-1 */
	 or	r7,r0,r11	/* store r11 into r7 for conversion */

/*
 * Now check for negative double precision sources. If the exponent is 30,
 * then convert the false alarm. If the exponent is 31, then check the
 * mantissa bits which correspond to integer bits. If any of them are a one,
 * then there is overflow. If they are zero, then check the guard, round,
 * and sticky bits.
 * Round toward zero and positive will not cause a roundup, but round toward
 * nearest and negative may, so perform those roundings. If there is no
 * overflow, then convert and return.
 */
checkdoubn:
	cmp	r11,r10,29		/* compare to 30, but exp. off by 1 */
	bb1	eq,r11,conversiondn	/* false alarm if exp. = 30 */
	extu	r10,r8,11<21>		/* check upper bits of lower mantissa */
	bcnd	ne0,r10,overflw		/* one of the bits is a 1, so oflow */
	extu	r10,r7,20<0>		/* check upper bits of upper mantissa */
	bcnd	ne0,r10,overflw		/* one of the bits is a 1, so oflow */
	bb0	rndlo,r1,possround	/* rounding mode is either round near */
					/* or round negative, which may cause */
					/* a round */
	br.n	FPintov_return		/* round positive, which will not */
					/* cause a round */
	 set	r6,r0,1<sign>
possround:
	extu	r12,r8,1<20>		/* get guard bit */
	extu	r11,r8,20<0>		/* get bits for sticky bit */
	bcnd.n	eq0,r11,nostickyn	/* do not set sticky */
	 mak	r12,r12,1<1>		/* set up field for branch table */
	set	r12,r12,1<0>		/* set sticky bit */
nostickyn:
	bb1	rndhi,r1,negative	/* rounding mode is negative */
nearest:
	cmp	r12,r12,3		/* are both guard and sticky set */
	bb1	eq,r12,overflw		/* both guard and sticky are set, */
					/* so signal overflow */
	or	r6,r0,r0		/* clear destination register r6 */
	br.n	FPintov_return
	 set	r6,r6,1<sign>		/* set the sign bit and take care of */
					/* this special case */
negative:
	bcnd	ne0,r12,overflw		/* -2**31 will be rounded to */
					/* -(2**31+1), so signal overflow */
	or	r6,r0,r0		/* clear destination register r6 */
	br.n	FPintov_return
	 set	r6,r6,1<sign>		/* set the sign bit and take care of */
					/* this special case */

	/*
	 * Since the exp. was 30, and there was no round-up, the largest
	 * number that S2 could have been was 2**31 - 1
	 */


	/* Convert the single precision positive floating point number. */

conversionsp:
	extu	r6,r8,3<29>	/* extract lower bits of integer */
	mak	r6,r6,3<7>	/* shift left to correct place in integer */
	mak	r10,r7,20<10>	/* shift left upper bits of integer */
	or	r6,r6,r10	/* form most of integer */
	br.n	FPintov_return
	 set	r6,r6,1<30>	/* set hidden one */

	/* Convert the single precision negative floating point number. */

conversionsn:
	bb1	eq,r11,exp31s	/* use old r11 to see if exp. is 31 */
	extu	r6,r8,3<29>	/* extract lower bits of mantissa */
	mak	r6,r6,3<7>	/* shift left to correct place in integer */
	mak	r10,r7,20<10>	/* shift left upper bits of integer */
	or	r6,r6,r10	/* form most of integer */
	set	r6,r6,1<30>	/* set hidden one */
	or.c	r6,r0,r6	/* negate result */
	br.n	FPintov_return
	 addu	r6,r6,1		/* add 1 to get 2''s complement */
exp31s:
	or	r6,r0,r0	/* clear r6 */
	br.n	FPintov_return
	 set	r6,r6,1<sign>	/* set sign bit */

	/* Convert the double precision positive floating point number. */

conversiondp:
	extu	r6,r8,10<22>	/* extract lower bits of integer */
	mak	r10,r7,20<10>	/* shift left upper bits of integer */
	or	r6,r6,r10	/* form most of integer */
	br.n	FPintov_return
	 set	r6,r6,1<30>	/* set hidden one */

	/*
	 * Convert the double precision negative floating point number.
	 * The number, whose exponent is 30, must be rounded before converting.
	 * Bits 4 and 3 are the rounding mode, and bits 2, 1, and 0 are the
	 * guard, round, and sticky bits for the branch table.
	 */

conversiondn:
	extu	r12,r8,1<22>	/* get LSB for integer with exp. = 30 */
	mak	r12,r12,1<2>	/* start to set up field for branch table */
	extu	r11,r8,1<21>	/* get guard bit */
	mak	r11,r11,1<1>	/* set up field for branch table */
	or	r12,r11,r12	/* set up field for branch table */
	extu	r11,r8,21<0>	/* get bits for sticky bit */
	bcnd	eq0,r11,nostkyn	/* do not set sticky */
	set	r12,r12,1<0>	/* set sticky bit */
nostkyn:
	rot	r11,r1,0<rndlo>	/* shift rounding mode to 2 LSB''s */
	mak	r11,r11,2<3>	/* set up field, clear other bits */
	or	r12,r11,r12	/* set up field for branch table */
	lda	r12,r0[r12]	/* scale r12 */
	or.u	r12,r12,hi16(ntable) /* load pointer into table */
	addu	r12,r12,lo16(ntable)
	jmp	r12

ntable:
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	naddone
	br	nnoaddone
	br	nnoaddone
	br	naddone
	br	naddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	naddone
	br	naddone
	br	naddone
	br	nnoaddone
	br	naddone
	br	naddone
	br	naddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone
	br	nnoaddone

/*
 * Add one to the mantissa, and check to see if it overflows to -2**31.
 * The conversion is done in nnoaddone.
 */

naddone:
	or	r10,r0,r0	/* clear r10 */
	set	r10,r10,1<22>	/* set LSB bit to 1 for adding */
	add.co	r8,r8,r10	/* add the 1 obtained from rounding */
	clr	r7,r7,12<20>	/* clear exponent and sign */
	add.ci	r7,r0,r7	/* add carry */
	bb1	20,r7,maxneg	/* rounded to -2**31,handle separately */
				/* the exponent was originally 30 */
nnoaddone:
	extu	r6,r8,11<22>	/* extract lower bits of integer */
	mak	r10,r7,20<10>	/* shift left upper bits of integer */
	or	r6,r6,r10	/* form most of integer */
	set	r6,r6,1<30>	/* set hidden one */
	or.c	r6,r0,r6	/* negate integer */
	br.n	FPintov_return
	 addu	r6,r6,1		/* add 1 to get 2''s complement */

maxneg:
	or	r6,r0,r0	/* clear integer */
	br.n	FPintov_return
	 set	r6,r6,1<sign>	/* set sign bit */

	/*
	 * For valid overflows, check to see if the integer overflow user
	 * handler is set. If it is set, then go to user handler, else write
	 * the correctly signed largest integer.
	 */

overflw:
	set	r2,r2,1<oper>
#ifdef HANDLER
	bb0	oper,r3,nohandler	/* do not go to user handler routine */
	bsr	_handler		/* go to user handler routine */
	br	FPintov_return
nohandler:
#endif
	bb0.n	sign,r7,FPintov_return	/* if positive then return */
	 set	r6,r6,31<0>		/* set result to largest positive int */
	or.c	r6,r0,r6		/* negate r6, giving largest negative */
					/* integer */

FPintov_return:
	ld	r1,r31,0		/* load return address from memory */
	jmp	r1

/*
 * Some instructions only have the S2 operations, so clear S1HI and S1LO
 * for those instructions so that the previous contents of S1HI and S1LO
 * do not influence this instruction.
 */

ASLOCAL(FPresoper)
	st	r1, r31, 0
	extu	r10,r9,5<11>	/* extract opcode */
#if 0
	cmp	r11,r10,FSQRTop	/* compare to FSQRT */
	bb1	eq,r11,S1clear	/* clear S1 if instruction only had S2 operand */
#endif
	cmp	r11,r10,INTop	/* compare to INT */
	bb1	eq,r11,S1clear	/* clear S1 if instruction only had S2 operand */
	cmp	r11,r10,NINTop	/* compare to NINT */
	bb1	eq,r11,S1clear	/* clear S1 if instruction only had S2 operand */
	cmp	r11,r10,TRNCop	/* compare to TRNC */
	bb0	eq,r11,opercheck /* check for reserved operands */

ASLOCAL(S1clear)
	or	r5,r0,r0	/* clear any NaN''s, denorms, or infinities */
	or	r6,r0,r0	/* that may be left in S1HI,S1LO from a */
				/* previous instruction */

/*
 * r12 contains the following flags:
 *   bit 9 -- s1sign
 *   bit 8 -- s2sign
 *   bit 7 -- s1nan
 *   bit 6 -- s2nan
 *   bit 5 -- s1inf
 *   bit 4 -- s2inf
 *   bit 3 -- s1zero
 *   bit 2 -- s2zero
 *   bit 1 -- s1denorm
 *   bit 0 -- s2denorm
 */

/*
 * Using code for both single and double precision, check if S1 is either
 * a NaN or infinity and set the appropriate flags in r12. Then check if
 * S2 is a NaN or infinity. If it is a NaN, then branch to the NaN routine.
 */

ASLOCAL(opercheck)
	extu	r10,r5,11<20>	/* internal representation for double */
	bb1.n	s1size,r9,S1NaNdoub /* S1 is double precision */
	 or	r12,r0,r0	/* clear operand flag register */
ASLOCAL(S1NaNsing)
	xor	r10,r10,0x0080	/* internal representation for single */
	ext	r10,r10,8<0>	/* precision is IEEE 8 bits sign extended */
				/* to 11 bits; for real exp. > 0, the */
				/* above instructions gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,127	/* Is exponent equal to IEEE 255 (here 127) */
	bb1	ne,r11,S2NaN	/* source 1 is not a NaN or infinity */
	mak	r10,r5,20<0>	/* load r10 with upper bits of S1 mantissa */
	extu	r11,r6,3<29>	/* get 3 upper bits of lower word */
	or	r11,r10,r11	/* combine any existing 1 */
	bcnd	eq0,r11,noS1NaNs /* since r11 can only hold 0 or a */
				/* > 0 number, branch to noS1NaN when eq0 */
	br.n	S2NaN		/* see if S2 has a NaN */
	 set	r12,r12,1<s1nan> /* indicate that S1 has a NaN */
ASLOCAL(noS1NaNs)
	br.n	S2NaN		/* check contents of S2 */
	 set	r12,r0,1<s1inf>	/* indicate that S1 has an infinity */

ASLOCAL(S1NaNdoub)
	xor	r10,r10,0x0400	/* precision is the same IEEE 11 bits */
				/* The above instructions gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,1023	/* Is exp. equal to IEEE 2047 (internal 1023) */
	bb1	ne,r11,S2NaN	/* source 1 is not a NaN or infinity */
	mak	r10,r5,20<0>	/* load r10 with upper bits of S1 mantissa */
	or	r11,r6,r10	/* combine existing 1''s of mantissa */
	bcnd	eq0,r11,noS1NaNd /* since r11 can only hold 0 or a > 0 */
				/* number, branch to noS1NaN when eq0 */
	br.n	S2NaN		/* see if S2 has a NaN */
	 set	r12,r12,1<s1nan> /* indicate that S1 has a NaN */
ASLOCAL(noS1NaNd)
	set	r12,r0,1<s1inf>	/* indicate that S1 has an infinity */

ASLOCAL(S2NaN)
	bb1.n	s2size,r9,S2NaNdoub /* S1 is double precision */
	 extu	r10,r7,11<20>	/* internal representation for double */
ASLOCAL(S2NaNsing)
	xor	r10,r10,0x0080	/* internal representation for single */
	ext	r10,r10,8<0>	/* precision is IEEE 8 bits sign extended */
				/* to 11 bits; for real exp. > 0, the */
				/* above instruction gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,127	/* Is exponent equal to IEEE 255 (here 127) */
	bb1	ne,r11,inf	/* source 2 is not a NaN or infinity */
	mak	r10,r7,20<0>	/* load r10 with upper bits of S1 mantissa */
	extu	r11,r8,3<29>	/* get 3 upper bits of lower word */
	or	r11,r10,r11	/* combine any existing 1''s */
	bcnd	eq0,r11,noS2NaNs /* since r11 can only hold 0 or a > 0 */
				/* number, branch to noS2NaNs when eq0 */
	br.n	_ASM_LABEL(NaN)	/* branch to NaN routine */
	 set	r12,r12,1<s2nan> /* indicate that s2 has a NaN */
ASLOCAL(noS2NaNs)
	bb0	s1nan,r12, 1f	/* branch to NaN if S1 is a NaN */
	br	_ASM_LABEL(NaN)
1:
	br.n	_ASM_LABEL(infinity) /* If S1 had a NaN we would have */
				/* already branched, and S2 does not have a */
				/* NaN, but it does have an infinity, so */
				/* branch to handle the finity */
	 set	r12,r12,1<s2inf> /* indicate that S2 has an infinity */

ASLOCAL(S2NaNdoub)
	xor	r10,r10,0x0400	/* precision is the same IEEE 11 bits */
				/* The above instruction gives a result exp. */
				/* that has the MSB flipped and sign */
				/* extended like in the IMPCR */
	cmp	r11,r10,1023	/* Is exp. equal to IEEE 2047 (internal 1023) */
	bb1	ne,r11,inf	/* source 2 is not a NaN or infinity */
	mak	r10,r7,20<0>	/* load r10 with upper bits of S2 mantissa */
	or	r11,r8,r10	/* combine existing 1''s of mantissa */
	bcnd	eq0,r11,noS2NaNd /* since r11 can only hold 0 or a > 0 */
				/* number, branch to noS2NaNd when eq0 */
	br.n	_ASM_LABEL(NaN)	/* branch to NaN routine */
	 set	r12,r12,1<s2nan> /* indicate that s2 has a NaN */
ASLOCAL(noS2NaNd)
	bb0	s1nan,r12,1f	/* branch to NaN if S1 is a NaN */
	br	_ASM_LABEL(NaN)
1:
	br.n	_ASM_LABEL(infinity) /* If S1 had a NaN we would have */
				/* already branched, and S2 does not have a */
				/* NaN, but it does have an infinity, so */
				/* branch to handle the finity */
	 set	r12,r12,1<s2inf> /* indicate that S2 has an infinity */

/*
 * If S2 was a NaN, the routine would have already branched to NaN. If S1
 * is a NaN, then branch to NaN. If S1 is not a NaN and S2 is infinity, then
 * we would have already branched to infinity. If S1 is infinity, then branch.
 * If the routine still has not branched, then branch to denorm, the only
 * reserved operand left.
 */

ASLOCAL(inf)
	bb0	s1nan,r12,1f	/* branch if S1 has a NaN and S2 does not */
	br	_ASM_LABEL(NaN)
1:
	bb0	s1inf,r12,2f	/* Neither S1 or S2 has a NaN, and we would */
				/* have branched already if S2 had an */
				/* infinity, so branch if S1 is infinity */
	br	_ASM_LABEL(infinity)
2:
	br	_ASM_LABEL(denorm)	/* branch to denorm, the only */
					/* remaining alternative */

/*
 * First check for an underflow user handler. If there is not one, then
 * branch to the routine to make a denormalized number. Before branching
 * to the underflow user handler, add 192 to a single precision exponent
 * and 1536 to a double precision exponent.
 */

ASLOCAL(FPunderflow)
	st	r1,r31,0	/* save return address */
	set	r2,r2,1<underflow>
	set	r2,r2,1<inexact>
#ifdef HANDLER
	bb0	efunf,r12,FPU_denorm	/* jump to default procedure */
	bb1	destsize,r12,doubleprec	/* double precision destination */
singleprec:
	or.u	r6,r0,0x0c00	/* load exponent adjust 192 */
	br.n	callundhand	/* branch to call handler for user handler */
	 add	r12,r6,r12	/* adjust single precision exponent */
doubleprec:
	or.u	r6,r0,0x6000	/* load exponent adjust 1536 */
	add	r12,r6,r12	/* adjust double precision exponent */
callundhand:
	bsr	_handler	/* call handler for user handler */
	br	Ureturn
#endif

/*
 * Now the floating point number, which has an exponent smaller than what
 * IEEE allows, must be denormalized. Denormalization is done by calculating
 * the difference between a denormalized exponent and an underflow exponent
 * and shifting the mantissa by that amount. A one may need to be subtracted
 * from the LSB if a one was added during rounding.
 * r9 is used to contain the guard, round, sticky, and an inaccuracy bit in
 * case some bits were shifted off the mantissa during denormalization.
 * r9 will contain:
 *   bit 4 -- new addone if one added during rounding after denormalization
 *   bit 3 -- inaccuracy flag caused by denormalization or pre-denormalization
 *            inexactness
 *   bit 2 -- guard bit of result
 *   bit 1 -- round bit of result
 *   bit 0 -- sticky bit of result
 */

FPU_denorm:
	bb1.n	destsize,r12,Udouble 	/* denorm for double */
	 extu	r9,r10,3<26>	/* load r9 with grs */
Usingle:
	mak	r5,r10,21<3>	/* extract high 21 bits of mantissa */
	extu	r6,r11,3<29>	/* extract low 3 bits of mantissa */
	or	r11,r5,r6	/* form 24 bits of mantissa */

/* See if the addone bit is set and unround if it is. */
	bb0.n	25,r10,nounrounds /* do not unround if addone bit clear */
	 extu	r6,r12,12<20>	/* extract signed exponent from IMPCR */
unrounds:
	subu	r11,r11,1	/* subtract 1 from mantissa */

/*
 * If the hidden bit is cleared after subtracting the one, then the one added
 * during the rounding must have propagated through the mantissa. The exponent
 * will need to be decremented.
 */
	bb1	23,r11,nounrounds /* if hidden bit is set,then exponent */
				/* does not need to be decremented */
decexps:
	sub	r6,r6,1		/* decrement exponent 1 */
	set	r11,r11,1<23>	/* set the hidden bit */

/*
 * For both single and double precision, there are cases where it is easier
 * and quicker to make a special case. Examples of this are if the shift
 * amount is only 1 or 2, or all the mantissa is shifted off, or all the
 * mantissa is shifted off and it is still shifting, or, in the case of
 * doubles, if the shift amount is around the boundary of MANTLO and MANTHI.
 */

nounrounds:
	or	r8,r0,lo16(0x00000f81)	/* load r8 with -127 in decimal */
					/* for lowest 12 bits */
	sub	r7,r8,r6	/* find difference between two exponents, */
				/* this amount is the shift amount */
	cmp	r6,r7,3		/* check to see if r7 contains 3 or more */
	bb1	ge,r6,threesing	/* br to code that handles shifts of >=3 */
	cmp	r6,r7,2		/* check to see if r7 contains 2 */
	bb1	eq,r6,twosing	/* br to code that handles shifts of 2 */
one:
	rot	r9,r9,0<1>	/* rotate roundoff register once, this places */
				/* guard in round and round in sticky */
	bb0	31,r9,nosticky1s /* do not or round and sticky if sticky is */
				/* 0, this lost bit will be cleared later */
	set	r9,r9,1<0>	/* or round and sticky */
nosticky1s:
	bb0	0,r11,guardclr1s /* do not set guard bit if LSB = 0 */
	set	r9,r9,1<2>	/* set guard bit */
guardclr1s:
	extu	r11,r11,31<1>	/* shift mantissa right 1 */
	br.n	round		/* round result */
	 mak	r9,r9,3<0>	/* clear bits lost during rotation */

twosing:
	rot	r9,r9,0<2>	/* rotate roundff register twice, this places */
				/* guard in sticky */
	bb0	30,r9,nosticky2s /* do not or guard and sticky if stick is 0 */
				/* this lost bit will be cleared later */
	br.n	noround2s	/* skip or old guard and old round if old */
				/* sticky set */
	 set	r9,r9,1<0>	/* or guard and sticky */
nosticky2s:
	bb0	31,r9,noround2s /* do not or guard and round if round is 0 */
				/* this lost bit will be cleared later */
	set	r9,r9,1<0>	/* or guard and round */
noround2s:
	bb0	0,r11,roundclr2s /* do not set round bit if LSB = 0 */
	set	r9,r9,1<1>	/* set round bit */
roundclr2s:
	bb0	1,r11,guardclr2s /* do not set guard bit if LSB + 1 = 0 */
	set	r9,r9,1<2>	/* set guard bit */
guardclr2s:
	extu	r11,r11,30<2>	/* shift mantissa right 2 */
	br.n	round		/* round result */
	 mak	r9,r9,3<0>	/* clear bits lost during rotation */

threesing:
	bb1	0,r9,noguard3s	/* check sticky initially */
				/* sticky is set, forget most of the oring */
nosticky3s:
	bb0	1,r9,noround3s	/* check round initially, do not set sticky */
	br.n	noguard3s	/* forget most of the rest of oring */
	 set	r9,r9,1<0>	/* if round is clear,set sticky if round set */
noround3s:
	bb0.n	2,r9,noguard3s	/* check guard initially, do not set sticky */
	 clr	r9,r9,2<1>	/* clear the original guard and round for when */
				/* you get to round section */
	set	r9,r9,1<0>	/* if guard is clear,set sticky if guard set */
noguard3s:
	cmp	r6,r7,23	/* check if # of shifts is <=23 */
	bb1	gt,r6,s24	/* branch to see if shifts = 24 */
	sub	r6,r7,2		/* get number of bits to check for sticky */
	mak	r6,r6,5<5>	/* shift width into width field */
	mak	r8,r11,r6	/* mask off shifted bits -2 */
	ff1	r8,r8		/* see if r8 has any ones */
	bb1	5,r8,nostky23	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky23:
	or	r8,r0,34	/* start code to get new mantissa plus two */
				/* extra bits for new round and new guard */
				/* bits */
	subu	r8,r8,r7
	mak	r8,r8,5<5>	/* shift field width into second five bits */
	extu	r6,r6,5<5>	/* shift previous shifted -2 into offset field */
	or	r6,r6,r8	/* complete field */
	extu	r11,r11,r6	/* form new mantissa with two extra bits */

	bb0	0,r11,nornd3s	/* do not set new round bit */
	set	r9,r9,1<1>	/* set new round bit */
nornd3s:
	bb0	1,r11,nogrd3s	/* do not set new guard bit */
	set	r9,r9,1<2>	/* set new guard bit */
nogrd3s:
	br.n	round		/* round mantissa */
	 extu	r11,r11,30<2>	/* shift off remaining two bits */

s24:
	cmp	r6,r7,24	/* check to see if # of shifts is 24 */
	bb1	gt,r6,s25	/* branch to see if shifts = 25 */
	bb1	0,r9,nostky24	/* skip checking if old sticky set */
	extu	r8,r11,22<0>	/* prepare to check bits that will be shifted */
				/* into the sticky */
	ff1	r8,r8		/* see if there are any 1''s */
	bb1	5,r8,nostky24	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky24:
	bb0	22,r11,nornd24	/* do not set new round bit */
	set	r9,r9,1<1>	/* set new round bit */
nornd24:
	set	r9,r9,1<2>	/* set new guard bit,this is hidden bit */
	br.n	round		/* round mantissa */
	 or	r11,r0,r0	/* clear r11, all of mantissa shifted off */

s25:
	cmp	r6,r7,25	/* check to see if # of shifts is 25 */
	bb1	gt,r6,s26	/* branch to execute for shifts => 26 */
	bb1	0,r9,nostky25	/* skip checking if old sticky set */
	extu	r8,r11,23<0>	/* prepare to check bits that will be shifted */
				/* into the sticky */
	ff1	r8,r8		/* see if there are any 1''s */
	bb1	5,r8,nostky25	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky25:
	set	r9,r9,1<1>	/* set new round bit,this is hidden bit */
	clr	r9,r9,1<2>	/* clear guard bit since nothing shifted in */
	br.n	round		/* round and assemble result */
	 or	r11,r0,r0	/* clear r11, all of mantissa shifted off */

s26:
	set	r9,r9,1<0>	/* set sticky bit,this contains hidden bit */
	clr	r9,r9,2<1>	/* clear guard and round bits since nothing */
				/* shifted in */
	br.n	round		/* round and assemble result */
	 or	r11,r0,r0	/* clear mantissa */

Udouble:
	mak	r5,r10,21<0>	/* extract upper bits of mantissa */
	bb0.n	25,r10,nounroundd /* do not unround if addone bit clear */
	 extu	r6,r12,12<20>	/* extract signed exponenet from IMPCR */
unroundd:
	or	r8,r0,1
	subu.co	r11,r11,r8	/* subtract 1 from mantissa */
	subu.ci	r5,r5,r0	/* subtract borrow from upper word */
	bb1	20,r5,nounroundd /* if hidden bit is set, then exponent does */
				/* not need to be decremented */
decexpd:
	sub	r6,r6,1		/* decrement exponent 1 */
	set	r5,r5,1<20>	/* set the hidden bit */

nounroundd:
	or	r8,r0,lo16(0x00000c01) /* load r8 with -1023 in decimal */
				/* for lowest 12 bits */
	sub	r7,r8,r6	/* find difference between two exponents, */
				/* this amount is the shift amount */
	cmp	r6,r7,3		/* check to see if r7 contains 3 or more */
	bb1	ge,r6,threedoub	/* br to code that handles shifts of >=3 */
	cmp	r6,r7,2		/* check to see if r7 contains 2 */
	bb1	eq,r6,twodoub	/* br to code that handles shifts of 2 */

onedoub:
	rot	r9,r9,0<1>	/* rotate roundoff register once, this places */
				/* guard in round and round in sticky */
	bb0	31,r9,nosticky1d/* do not or round and sticky if sticky is 0 */
				/* this lost bit will be cleared later */
	set	r9,r9,1<0>	/* or old round and old sticky into new sticky */
nosticky1d:
	bb0	0,r11,guardclr1d /* do not set new guard bit if old LSB = 0 */
	set	r9,r9,1<2>	/* set new guard bit */
guardclr1d:
	extu	r11,r11,31<1>	/* shift lower mantissa over 1 */
	mak	r6,r5,1<31>	/* shift off low bit of high mantissa */
	or	r11,r6,r11	/* load high bit onto lower mantissa */
	extu	r5,r5,20<1>	/* shift right once upper 20 bits of mantissa */
	br.n	round		/* round mantissa and assemble result */
	 mak	r9,r9,3<0>	/* clear bits lost during rotation */

twodoub:
	rot	r9,r9,0<2>	/* rotate roundoff register twice, this places */
				/* old guard into sticky */
	bb0	30,r9,nosticky2d /* do not or old guard and old sticky if */
				/* old sticky is 0 */
	br.n	noround2d	/* skip or of old guard and old round if old */
				/* sticky set */
	 set	r9,r9,1<0>	/* or old guard and old sticky into new sticky */
nosticky2d:
	bb0	31,r9,noround2d	/* do not or old guard and old round if */
				/* old round is 0 */
	set	r9,r9,1<0>	/* or old guard and old round into new sticky */
noround2d:
	bb0	0,r11,roundclr2d /* do not set round bit if old LSB = 0 */
	set	r9,r9,1<1>	/* set new round bit */
roundclr2d:
	bb0	1,r11,guardclr2d /* do not set guard bit if old LSB + 1 = 0 */
	set	r9,r9,1<2>	/* set new guard bit */
guardclr2d:
	extu	r11,r11,30<2>	/* shift lower mantissa over 2 */
	mak	r6,r5,2<30>	/* shift off low bits of high mantissa */
	or	r11,r6,r11	/* load high bit onto lower mantissa */
	extu	r5,r5,19<2>	/* shift right twice upper 19 bits of mantissa */
	br.n	round		/* round mantissa and assemble result */
	 mak	r9,r9,3<0>	/* clear bits lost during rotation */

threedoub:
	bb1	0,r9,noguard3d	/* checky sticky initially */
				/* sticky is set, forget most of rest of oring */
nosticky3d:
	bb0	1,r9,noround3d	/* check old round, do not set sticky if */
				/* old round is clear, set otherwise */
	br.n	noguard3d	/* sticky is set, forget most of rest of oring */
	 set	r9,r9,1<0>	/* set sticky if old round is set */
noround3d:
	bb0	2,r9,noguard3d	/* check old guard, do not set sticky if 0 */
	clr	r9,r9,2<1>	/* clear the original guard and round for when */
				/* you get to round section */
	set	r9,r9,1<0>	/* set sticky if old guard is set */
noguard3d:
	cmp	r6,r7,32	/* do I need to work with a 1 or 2 word mant. */
				/* when forming sticky, round and guard */
	bb1	gt,r6,d33	/* jump to code that handles 2 word mantissas */
	sub	r6,r7,2		/* get number of bits to check for sticky */
	mak	r6,r6,5<5>	/* shift width into width field */
	mak	r8,r11,r6	/* mask off shifted bits -2 */
	ff1	r8,r8		/* see if r8 has any ones */
	bb1	5,r8,nostky32	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky32:
	or	r8,r0,34	/* start code to get new mantissa plus two */
				/* extra bits for new round and new guard bits, */
				/* the upper word bits will be shifted after */
				/* the round and guard bits are handled */
	subu	r8,r8,r7
	mak	r8,r8,5<5>	/* shift field width into second five bits */
	extu	r6,r6,5<5>	/* shift previous shifted -2 into offset field */
	or	r6,r6,r8	/* complete bit field */
	extu	r11,r11,r6	/* partially form new low mantissa with 2 more */
				/* bits */
	bb0	0,r11,nornd32d	/* do not set new round bit */
	set	r9,r9,1<1>	/* set new round bit */
nornd32d:
	bb0	1,r11,nogrd32d	/* do not set new guard bit */
	set	r9,r9,1<2>	/* set new guard bit */
nogrd32d:
	extu	r11,r11,30<2>	/* shift off remaining two bits */
	mak	r6,r7,5<5>	/* shift field width into second 5 bits, if the */
				/* width is 32, then these bits will be 0 */
	or	r8,r0,32	/* load word length into r8 */
	sub	r8,r8,r7	/* form offset for high bits moved to low word */
	or	r6,r6,r8	/* form complete bit field */
	mak	r6,r5,r6	/* get shifted bits of high word */
	or	r11,r6,r11	/* form new low word of mantissa */
	bcnd	ne0,r8,regular33 /* do not adjust for special case of r8 */
	br.n	round		/* containing zeros, which would cause */
	 or	r5,r0,r0	/* all of the bits to be extracted under */
				/* the regular method */
regular33:
	mak	r6,r7,5<0>	/* place lower 5 bits of shift into r6 */
	mak	r8,r8,5<5>	/* shift r8 into width field */
	or	r6,r6,r8	/* form field for shifting of upper bits */
	br.n	round		/* round and assemble result */
	 extu	r5,r5,r6	/* form new high word mantissa */

d33:
	cmp	r6,r7,33	/* is the number of bits to be shifted is 33? */
	bb1	gt,r6,d34	/* check to see if # of bits is 34 */
	bb1	0,r9,nostky33	/* skip checking if old sticky set */
	mak	r6,r11,31<0>	/* check bits that will be shifted into sticky */
	ff1	r8,r8		/* check for ones */
	bb1	5,r8,nostky33	/* do not set sticky if there are no ones */
	set	r9,r9,1<0>	/* set new sticky bit */
nostky33:
	bb0	31,r11,nornd33	/* do not set round if bit is not a 1 */
	set	r9,r9,1<1>	/* set new round bit */
nornd33:
	bb0	0,r5,nogrd33	/* do not set guard bit if bit is not a 1 */
	set	r9,r9,1<2>	/* set new guard bit */
nogrd33:
	extu	r11,r5,31<1>	/* shift high bits into low word */
	br.n	round		/* round and assemble result */
	 or	r5,r0,r0	/* clear high word */

d34:
	cmp	r6,r7,34	/* is the number of bits to be shifted 34? */
	bb1	gt,r6,d35	/* check to see if # of bits is >= 35 */
	bb1	0,r9,nostky34	/* skip checking if old sticky set */
	ff1	r8,r11		/* check bits that will be shifted into sticky */
	bb1	5,r8,nostky34	/* do not set sticky if there are no ones */
	set	r9,r9,1<0>	/* set new sticky bit */
nostky34:
	bb0	0,r5,nornd34	/* do not set round if bit is not a 1 */
	set	r9,r9,1<1>	/* set new round bit */
nornd34:
	bb0	1,r5,nogrd34	/* do not set guard bit if bit is not a 1 */
	set	r9,r9,1<2>	/* set new guard bit */
nogrd34:
	extu	r11,r5,30<2>	/* shift high bits into low word */
	br.n	round		/* round and assemble result */
	 or	r5,r0,r0	/* clear high word */

d35:
	cmp	r6,r7,52	/* see if # of shifts is 35 <= X <= 52 */
	bb1	gt,r6,d53	/* check to see if # of shifts is 52 */
	bb1.n	0,r9,nostky35	/* skip checking if old sticky set */
	 sub	r7,r7,34	/* subtract 32 from # of shifts so that opera- */
				/* tions can be done on the upper word, and */
				/* then subtract two more checking guard and */
				/* sticky bits */
	ff1	r8,r11		/* see if lower word has a bit for sticky */
	bb1	5,r8,stkycheck35 /* see if upper word has any sticky bits	*/
	br.n	nostky35	/* quit checking for sticky */
	 set	r9,r9,1<0>	/* set sticky bit */
stkycheck35:
	mak	r6,r7,5<5>	/* place width into width field */
	mak	r8,r5,r6	/* mask off shifted bits - 2 */
	ff1	r8,r8		/* see if r8 has any ones */
	bb1	5,r8,nostky35	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky35:
	or	r8,r0,32	/* look at what does not get shifted off plus */
				/* round and sticky, remember that the r7 value */
				/* was adjusted so that it did not include */
				/* new round or new sticky in shifted off bits */
	subu	r8,r8,r7	/* complement width */
	mak	r8,r8,5<5>	/* shift width into width field */
	or	r8,r7,r8	/* add offset field */
	extu	r11,r5,r8	/* extract upper bits into low word */
	bb0	0,r11,nornd35	/* do not set new round bit */
	set	r9,r9,1<1>	/* set new round bit */
nornd35:
	bb0	1,r11,nogrd35	/* do not set new guard bit */
	set	r9,r9,1<2>	/* set new guard bit */
nogrd35:
	extu	r11,r11,30<2>	/* shift off remaining guard and round bits */
	br.n	round		/* round and assemble result */
	 or	r5,r0,r0	/* clear high word */

d53:
	cmp	r6,r7,53	/* check to see if # of shifts is 53 */
	bb1	gt,r6,d54	/* branch to see if shifts = 54 */
	bb1	0,r9,nostky53	/* skip checking if old sticky set */
	ff1	r8,r11		/* see if lower word has a bit for sticky */
	bb1	5,r8,stkycheck53 /* see if upper word has any sticky bits	*/
	br.n	nostky53	/* quit checking for sticky */
	 set	r9,r9,1<0>	/* set sticky bit */
stkycheck53:
	mak	r6,r5,19<0>	/* check bits that are shifted into sticky */
	ff1	r8,r6		/* see if r6 has any ones */
	bb1	5,r8,nostky53	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky53:
	bb0	19,r5,nornd53	/* do not set new round bit */
	set	r9,r9,1<1>	/* set new round bit */
nornd53:
	set	r9,r9,1<2>	/* set new guard bit,this is hidden bit */
	or	r5,r0,r0	/* clear high word */
	br.n	round		/* round and assemble result */
	 or	r11,r0,r0	/* clear low word */

d54:
	cmp	r6,r7,54	/* check to see if # of shifts is 54 */
	bb1	gt,r6,d55	/* branch to execute for shifts =>55 */
	bb1	0,r9,nostky54	/* skip checking if old sticky set */
	ff1	r8,r11		/* see if lower word has a bit for sticky */
	bb1	5,r8,stkycheck54 /* see if upper word has any sticky bits	*/
	br.n	nostky54	/* quit checking for sticky */
	 set	r9,r9,1<0>	/* set sticky bit */
stkycheck54:
	mak	r6,r5,20<0>	/* check bits that are shifted into sticky */
	ff1	r8,r6		/* see if r6 has any ones */
	bb1	5,r8,nostky54	/* do not set sticky if no ones found */
	set	r9,r9,1<0>	/* set sticky bit */
nostky54:
	set	r9,r9,1<1>	/* set new round bit,this is hidden bit */
	clr	r9,r9,1<2>	/* clear guard bit since nothing shifted in */
	or	r5,r0,r0	/* clear high word */
	br.n	round		/* round and assemble result */
	 or	r11,r0,r0	/* clear low word */

d55:
	set	r9,r9,1<0>	/* set new sticky bit,this contains hidden bit */
	clr	r9,r9,2<1>	/* clear guard and round bits since nothing */
				/* shifted in */
	or	r5,r0,r0	/* clear high word */
	or	r11,r0,r0	/* clear low word */


/* The first item that the rounding code does is see if either guard, round, */
/* or sticky is set. If all are clear, then there is no denormalization loss */
/* and no need to round, then branch to assemble answer. */
/* For rounding, a branch table is set up. The left two most bits are the */
/* rounding mode. The third bit is either the LSB of the mantissa or the */
/* sign bit, depending on the rounding mode. The three LSB''s are the guard, */
/* round and sticky bits. */

round:
	ff1	r8,r9		/* see if there is denormalization loss */
	bb1	5,r8,assemble	/* no denormalization loss or inexactness */
	extu	r6,r10,2<modelo> /* extract rounding mode */
	bb1.n	modehi,r10,signext /* use sign bit instead of LSB */
	 mak	r6,r6,2<4>	/* shift over rounding mode */
	extu	r7,r11,1<0>	/* extract LSB */
	br.n	grs		/* skip sign extraction */
	 mak	r7,r7,1<3>	/* shift over LSB */
signext:
	extu	r7,r10,1<31>	/* extract sign bit */
	mak	r7,r7,1<3>	/* shift sign bit over */
grs:
	or	r6,r6,r7
	or	r6,r6,r9	/* or in guard, round, and sticky */
	or.u	r1,r0,hi16(roundtable) /* form address of branch table */
	or	r1,r1,lo16(roundtable)
	lda	r6,r1[r6]	/* scale offset into branch table */
	jmp.n	r6		/* jump to branch table */
	 set	r9,r9,1<3>	/* set inexact flag in r9 */

roundtable:
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	addone
	br	addone
	br	addone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	addone
	br	addone
	br	addone
	br	addone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	noaddone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	addone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone
	br	noaddone

/* Round by adding a one to the LSB of the mantissa. */
addone:
	or	r6,r0,1		/* load a 1 into r6 so that add.co can be used */
	add.co	r11,r11,r6	/* add a one to the lower word of result */
	bb0.n	destsize,r12,noaddone /* single result,forget carry */
	 set	r9,r9,1<4>	/* indicate that a 1 has been added */
	add.ci	r5,r5,r0	/* propagate carry into high word */


/* Branch to inexact user handler if there is one. */

noaddone:
	set	r2,r2,1<inexact>
	set	r2,r2,1<underflow>
#ifdef HANDLER
	bb1	efinx,r12,modformdef	/* branch to modify form for user */
					/* handler */
#endif


/* Assemble the result of the denormalization routine for writeback to the */
/* destination register. The exponent of a denormalized number is zero, */
/* so simply assemble the sign and the new mantissa. */

assemble:
	bb1	destsize,r12,doubassem	/* assemble double result */
	bb0	sign,r10,exassems	/* exit assemble if sign is zero */
	set	r11,r11,1<sign>		/* make result negative */
exassems:
	br	Ureturn	

doubassem:
	bb0.n	sign,r10,signclr	/* do not set sign in r10 */
	 or	r10,r5,r0		/* load high word from r5 into r10 */
	set	r10,r10,1<sign>		/* high word with sign loaded */
signclr:
	br	Ureturn


/* modfordef modifies the result of denormalization to the input format of */
/* the inexact user handler. This input format is the same format that */
/* MANTHI, MANTLO, and IMPCR were initially loaded with. */

#ifdef HANDLER
modformdef:
	clr	r12,r12,12<20>	/* clear result exponent,IMPCR complete */
	clr	r10,r10,4<25>	/* clear old guard,round,sticky,and addone */
	mak	r5,r9,3<26>	/* make grs field */
	bb0.n	4,r9,newaddone	/* do not set new addone in MANTHI */
	 or	r10,r5,r10	/* or in new grs field */
	set	r10,r10,1<25>	/* set new addone */
newaddone:
	bb1.n	destsize,r12,moddefd /* branch to handle double precision */
	 clr	r10,r10,21<0>	/* clear upper bits of old mantissa */
moddefs:
	extu	r5,r11,20<3>	/* extract upper bits */
	or	r10,r5,r10	/* MANTHI complete */
	bsr.n	_handler	/* execute user handler for inexact */
	 rot	r11,r11,0<3>	/* MANTLO complete */
	br	Ureturn
moddefd:
	bsr.n	_handler	/* execute user handler for inexact */
	 or	r10,r5,r10	/* MANTHI complete,r5 should be set to OR */
#endif

/* Return to fpui. */

Ureturn:
	ld	r1,r31,0	/* load return address */
	jmp	r1

/*
 * FPoverflow
 */
/* If the overflow user handler bit is not set, then the inexact bit in the */
/* FPSR is set, and the inexact user handler bit is checked. If it is set, */
/* then the inexact user handler is executed, else the default routine for */
/* overflow is executed. */

ASLOCAL(FPoverflow)
	st	r1,r31,0	/* save return address */
	set	r2,r2,1<overflow>
	set	r2,r2,1<inexact>
#ifdef HANDLER
	bb1	efovf,r12,hand	/* go to user handler if bit set for overflow */
	bb0	efinx,r12,nohandler/* if userhandler for inexact not set,then */
				/* round result */
	br	callhandler	/* branch to user handler for inexact */

/* Before the overflow user handler is executed, the exponent is modified */
/* by subtracting 192 for single precision and 1536 for double precision. */

hand:
	bb1	10,r12,doubleprec /* double precision result */
singleprec:
	or.u	r5,r0,0x0c00	/* load exponent adjust */
	br.n	callhandler	/* prepare to call user handler */
	 subu	r12,r12,r5	/* adjust single precision exponent */
doubleprec:
	or.u	r5,r0,0x6000	/* load exponent adjust */
	subu	r12,r12,r5	/* adjust double precision exponent */
callhandler:
	bsr	_handler	/* branch to common handler routine */
	br	return
#endif

/* Determine which rounding mode to use for the default procedure. */

nohandler:
	bb1	modehi,r10,signed /* mode is either round toward pos. or neg. */
	bb0	modelo,r10,OFnearest /* rounding mode is round nearest */
	br	OFzero		/* rounding mode is round zero */
signed:
	bb0	modelo,r10,OFnegative /* rounding mode is round negative */
	br	positive	/* rounding mode is round positive */


/* In the round toward nearest mode, positive values are rounded to */
/* positive infinity and negative values are loaded toward negative infinity. */
/* The value for single or double precision is loaded from a data table. */

OFnearest:
	bb1.n	destsize,r12,neardouble	/* branch to neardouble of */
					/* double result */
	 mask.u	r5,r10,0x8000		/* mask off sign bit from MANTHI */
	or.u	r11,r0,hi16(0x7f800000)	/* load single infinity constant */
	or	r11,r11,lo16(0x7f800000)
	br.n	return			/* return with result */
	 or	r11,r5,r11		/* adjust sign */
neardouble:
	or	r11,r0,r0		/* load lower word of infinity */
	or.u	r10,r0,hi16(0x7ff00000)	/* load upper word of infinity */
	or	r10,r10,lo16(0x7ff00000)
	br.n	return			/* return with result */
	 or	r10,r5,r10		/* adjust sign */


/* In the round toward zero mode, positive values are rounded to the largest */
/* postive finite number and negative values are rounded toward the largest */
/* negative finite number. */
/* The value for single or double precision is loaded from a data table. */

OFzero:
	bb1.n	destsize,r12,zerodouble	/* branch to zerodouble of */
					/* double result */
	 mask.u	r5,r10,0x8000		/* mask off sign bit from MANTHI */
	or.u	r11,r0,hi16(0x7f7fffff)	/* load single finite number constant */
	or	r11,r11,lo16(0x7f7fffff)
	br.n	return			/* return with result */
	 or	r11,r5,r11		/* adjust sign */
zerodouble:
	set	r11,r0,0<0>		/* load lower word of finite number */
	or.u	r10,r0,hi16(0x7fefffff)	/* load upper word of finite number */
	or	r10,r10,lo16(0x7fefffff)
	br.n	return			/* return with result */
	 or	r10,r5,r10		/* adjust sign */


/* In the round toward positve mode, positive values are rounded to */
/* postive infinity and negative values are loaded toward the largest */
/* negative finite number. */
/* The value for single or double precision is loaded from a data table. */

positive:
	bb1	destsize,r12,posdouble	/* branch to section for double result */
possingle:
	bb1	sign,r10,possingleneg	/* branch to section for negatives */
possinglepos:
	or.u	r11,r0,hi16(0x7f800000)	/* load single infinity constant */
	br.n	return			/* return with result */
	 or	r11,r11,lo16(0x7f800000)
possingleneg:
	or.u	r11,r0,hi16(0x7f7fffff)	/* load single finite number constant */
	or	r11,r11,lo16(0x7f7fffff)
	br.n	return			/* return with result */
	 set	r11,r11,1<sign>		/* set sign for negative */
posdouble:
	bb1	sign,r10,posdoubleneg	/* branch to negative double results */
posdoublepos:
	or	r11,r0,r0		/* load lower word of double infinity */
	or.u	r10,r0,hi16(0x7ff00000)	/* load upper word of infinity */
	br.n	return			/* return with result */
	 or	r10,r10,lo16(0x7ff00000)
posdoubleneg:
	set	r11,r0,0<0>		/* load lower word of finite number */
	or.u	r10,r0,hi16(0x7fefffff)	/* load upper word of finite number */
	or	r10,r10,lo16(0x7fefffff)
	br.n	return			/* return with result */
	 set	r10,r10,1<sign>		/* set sign for negative */


/* In the round toward negative mode, positive values are rounded to the largest */
/* postive finite number and negative values are rounded to negative infinity. */
/* The value for single or double precision is loaded from a data table. */

OFnegative:
	bb1	destsize,r12,negdouble	/* branch to section for double result */
negsingle:
	bb1	sign,r10,negsingleneg	/* branch to section for negatives */
negsinglepos:
	or.u	r11,r0,hi16(0x7f7fffff)	/* load single finite number constant */
	br.n	return			/* return with result */
	 or	r11,r11,lo16(0x7f7fffff)
negsingleneg:
	or.u	r11,r0,hi16(0x7f800000)	/* load single infinity constant */
	or	r11,r11,lo16(0x7f800000)
	br.n	return			/* return with result */
	 set	r11,r11,1<sign>		/* set sign for negative */
negdouble:
	bb1	sign,r10,negdoubleneg	/* branch to negative double results */
negdoublepos:
	set	r11,r0,0<0>		/* load lower word of finite number */
	or.u	r10,r0,hi16(0x7fefffff)	/* load upper word of finite number */
	br.n	return			/* return with result */
	 or	r10,r10,lo16(0x7fefffff)
negdoubleneg:
	or	r11,r0,r0		/* load lower word of double infinity */
	or.u	r10,r0,hi16(0x7ff00000)	/* load upper word of infinity */
	or	r10,r10,lo16(0x7ff00000)
	set	r10,r10,1<sign>		/* set sign for negative */

return:
	ld	r1,r31,0		/* ld return address */
	jmp	r1

	data


/* If either S1 or S2 is a signalling NaN, then set the invalid operation */
/* bit of the FPSR. If the invalid operation user handler flag is set and */
/* then NaN is signalling, then branch to the handler routine to go to the */
/* user handler. */
/* If S1 is the only NaN or one of two NaN''s, then write */
/* a quiet S1 to the result. A signalling NaN must be made quiet before */
/* it can be written, but a signalling S2 is not modified in this routine */
/* if S1 is a NaN. */
	text
ASLOCAL(NaN)
	bb0.n	s1nan,r12,S2sigcheck	/* S1 is not a NaN */
	 st	r1,r31,0		/* save return address */
	bb1	sigbit,r5,S2sigcheck	/* S1 is not a signaling NaN */
	set	r2,r2,1<oper>
#ifdef HANDLER
	bb0	oper,r3,S1nohandler	/* branch if no user handler */
	bsr	_handler		/* branch to handler */
	br	FPnan_return
ASLOCAL(S1nohandler)
#endif
	br.n	S1write		/* FPSR bit already set, S1 is made quiet, */
				/* and since we always write S1 if it is a */
				/* NaN, write S1 and skip rest of routine */
	 set	r5,r5,1<sigbit>	/* make S1 a quiet NaN */

ASLOCAL(S2sigcheck)
	bb0	s2nan,r12,S1write	/* S2 is not a NaN */
	bb1	sigbit,r7,S1write	/* S2 is not a signaling NaN */
	set	r2,r2,1<oper>
#ifdef HANDLER
	bb0	oper,r3,S2nohandler	/* branch if no user handler */
	bsr	_handler		/* branch to handler */
	br	FPnan_return
#endif

ASLOCAL(S2nohandler)
	set	r7,r7,1<sigbit>	/* make S2 a quiet NaN */


/* Write a single or double precision quiet NaN unless the opeation is FCMP. */
/* If the operation is FCMP, then set the not comparable bit in the result. */

ASLOCAL(S1write)
	bb0	s1nan,r12,S2write /* do not write S1 if it is not a NaN */
	extu	r10,r9,5<11>	/* extract opcode */
	cmp	r11,r10,FCMPop	/* compare to FCMP */
	bb1	ne,r11,S1noFCMP	/* operation is not FCMP */
	set	r6,r0,1<nc>	/* set the not comparable bit */
	br.n	FPnan_return
	 set	r6,r6,1<ne>	/* set the not equal bit */
ASLOCAL(S1noFCMP)
	bb1.n	dsize,r9,wrdoubS1 /* double destination */
	 set	r5,r5,11<20>	/* set all exponent bits to 1 */
/* The single result will be formed the same way whether S1 is a single or double */
ASLOCAL(wrsingS1)
	mak	r10,r5,28<3>	/* wipe out extra exponent bits */
	extu	r11,r6,3<29>	/* get lower three bits of mantissa */
	or	r10,r10,r11	/* combine all of result except sign */
	clr	r6,r5,31<0>	/* clear all but sign */
	br.n	FPnan_return
	 or	r6,r6,r10	/* form result */

ASLOCAL(wrdoubS1)
	set	r6,r6,29<0>	/* set extra bits of lower word */
	br	FPnan_return	/* no modification necessary for writing */
				/* double to double, so return */

ASLOCAL(S2write)
	extu	r10,r9,5<11>	/* extract opcode */
	cmp	r11,r10,FCMPop	/* compare to FCMP */
	bb1.n	ne,r11,S2noFCMP	/* operation is not FCMP */
	 set	r7,r7,11<20>	/* set all exponent bits to 1 */
	set	r6,r0,1<nc>	/* set the not comparable bit */
	br.n	FPnan_return
	 set	r6,r6,1<ne>	/* set the not equal bit */
ASLOCAL(S2noFCMP)
	bb1.n	dsize,r9,wrdoubS2 /* double destination */
	 set	r5,r5,11<20>	/* set all exponent bits to 1 */
/* The single result will be formed the same way whether S1 is a single or double */
ASLOCAL(wrsingS2)
	mak	r10,r7,28<3>	/* wipe out extra exponent bits */
	extu	r11,r8,3<29>	/* get lower three bits of mantissa */
	or	r10,r10,r11	/* combine all of result except sign */
	clr	r6,r7,31<0>	/* clear all but sign */
	br.n	FPnan_return
	 or	r6,r6,r10	/* form result */

ASLOCAL(wrdoubS2)
	set	r6,r8,29<0>	/* set extra bits of lower word */

/* Return from this subroutine with the result. */

ASLOCAL(FPnan_return)
				/* no modification necessary for writing */
				/* double to double, so return */
	ld	r1,r31, 0	/* retrieve return address */
	jmp	r1

	data

/*
 * infinity
 */

/* Extract the opcode, compare to a constant, and branch to the code */
/* for the instruction. */

ASLOCAL(infinity)
	extu	r10,r9,5<11>	/* extract opcode */
	cmp	r11,r10,FADDop	/* compare to FADD */
	bb1.n	eq,r11,FADD	/* operation is FADD */
	 st	r1,r31,0	/* save return address */
	cmp	r11,r10,FSUBop	/* compare to FSUB */
	bb1	eq,r11,FSUB	/* operation is FSUB */
	cmp	r11,r10,FCMPop	/* compare to FCMP */
	bb1	eq,r11,FCMP	/* operation is FCMP */
	cmp	r11,r10,FMULop	/* compare to FMUL */
	bb1	eq,r11,FMUL	/* operation is FMUL */
	cmp	r11,r10,FDIVop	/* compare to FDIV */
	bb1	eq,r11,FDIV	/* operation is FDIV */
#if 0
	cmp	r11,r10,FSQRTop	/* compare to FSQRT */
	bb1	eq,r11,FSQRT	/* operation is FSQRT */
#endif
	cmp	r11,r10,INTop	/* compare to INT */
	bb1	eq,r11,FP_inf_overflw /* operation is INT */
	cmp	r11,r10,NINTop	/* compare to NINT */
	bb1	eq,r11,FP_inf_overflw /* operation is NINT */
	cmp	r11,r10,TRNCop	/* compare to TRNC */
	bb1	eq,r11,FP_inf_overflw /* operation is TRNC */


/* Adding infinities of opposite signs will cause an exception, */
/* but all other operands will result in a correctly signed infinity. */

FADD:
	bb0	s1inf,r12,addS2write	/* branch if S1 not infinity */
	bb0	s2inf,r12,addS1write	/* S2 is not inf., so branch to write S1 */
	bb1	sign,r5,addS1neg	/* handle case of S1 negative */
addS1pos:
	bb1	sign,r7,excpt		/* adding infinities of different */
					/* signs causes an exception */
	br	poswrinf		/* branch to write positive infinity */
addS1neg:
	bb0	sign,r7,excpt		/* adding infinities of different */
					/* signs causes an exception */
	br	negwrinf		/* branch to write negative infinity */
addS1write:
	bb0	sign,r5,poswrinf	/* branch to write positive infinity */
	br	negwrinf		/* branch to write negative infinity */
addS2write:
	bb0	sign,r7,poswrinf	/* branch to write positive infinity */
	br	negwrinf		/* branch to write negative infinity */


/* Subtracting infinities of the same sign will cause an exception, */
/* but all other operands will result in a correctly signed infinity. */

FSUB:
	bb0	s1inf,r12,subS2write	/* branch if S1 not infinity */
	bb0	s2inf,r12,subS1write	/* S2 is not inf., so branch to write S1 */
	bb1	sign,r5,subS1neg	/* handle case of S1 negative */
subS1pos:
	bb0	sign,r7,excpt		/* subtracting infinities of the same */
					/* sign causes an exception */
	br	poswrinf		/* branch to write positive infinity */
subS1neg:
	bb1	sign,r7,excpt		/* subtracting infinities of the same */
					/* sign causes an exception */
	br	negwrinf		/* branch to write negative infinity */
subS1write:
	bb0	sign,r5,poswrinf	/* branch to write positive infinity */
	br	negwrinf		/* branch to write negative infinity */
subS2write:
	bb1	sign,r7,poswrinf	/* branch to write positive infinity */
	br	negwrinf		/* branch to write negative infinity */


/* Compare the operands, at least one of which is infinity, and set the */
/* correct bits in the destination register. */

FCMP:
	bb0.n	s1inf,r12,FCMPS1f	/* branch for finite S1 */
	 set	r4,r0,1<cp>		/* since neither S1 or S2 is a NaN, */
					/* set cp */
FCMPS1i:
	bb1	sign,r5,FCMPS1ni	/* branch to negative S1i */
FCMPS1pi:
	bb0	s2inf,r12,FCMPS1piS2f	/* branch to finite S2 with S1pi */
FCMPS1piS2i:
	bb1	sign,r7,FCMPS1piS2ni	/* branch to negative S2i with S1pi */
FCMPS1piS2pi:
	set	r4,r4,1<eq>		/* set eq bit */
	set	r4,r4,1<le>		/* set le bit */
	set	r4,r4,1<ge>		/* set ge bit */
	set	r4,r4,1<ib>		/* set ib bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1piS2ni:
	set	r4,r4,1<ne>		/* set ne bit */
	set	r4,r4,1<gt>		/* set gt bit */
	br.n	move
	 set	r4,r4,1<ge>		/* set ge bit */
FCMPS1piS2f:
	set	r4,r4,1<ne>		/* set ne bit */
	set	r4,r4,1<gt>		/* set gt bit */
	bsr.n	_ASM_LABEL(zero)	/* see if any of the operands are zero */
	 set	r4,r4,1<ge>		/* set ge bit */
	bb0	s2zero,r12,FCMPS1piS2nz	/* check for negative if s2 not zero */
	set	r4,r4,1<ou>		/* set ou bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1piS2nz:
	bb1	sign,r7,move		/* return if s2 is negative */
FCMPS1piS2pf:
	set	r4,r4,1<ou>		/* set ou bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1ni:
	bb0	s2inf,r12,FCMPS1niS2f	/* branch to finite S2 with S1ni */
FCMPS1niS2i:
	bb1	sign,r7,FCMPS1niS2ni	/* branch to negative S2i with S1ni */
FCMPS1niS2pi:
	set	r4,r4,1<ne>		/* set eq bit */
	set	r4,r4,1<le>		/* set le bit */
	set	r4,r4,1<lt>		/* set lt bit */
	set	r4,r4,1<ou>		/* set ou bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1niS2ni:
	set	r4,r4,1<eq>		/* set eq bit */
	set	r4,r4,1<le>		/* set le bit */
	br.n	move
	 set	r4,r4,1<ge>		/* set ge bit */
FCMPS1niS2f:
	set	r4,r4,1<ne>		/* set eq bit */
	set	r4,r4,1<le>		/* set le bit */
	bsr.n	_ASM_LABEL(zero)	/* see if any of the operands are zero */
	 set	r4,r4,1<lt>		/* set lt bit */
	bb0	s2zero,r12,FCMPS1niS2nz	/* branch if s2 is not zero */
	set	r4,r4,1<ou>		/* set ou bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1niS2nz:
	bb1	sign,r7,move		/* return if s2 is negative */
	set	r4,r4,1<ou>		/* set ou bit */
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1f:
	bb1	sign,r5,FCMPS1nf	/* branch to negative S1f */
FCMPS1pf:
	bb1.n	sign,r7,FCMPS1pfS2ni	/* branch to negative S2i with S1pf */
	 set	r4,r4,1<ne>		/* set ne bit */
FCMPS1pfS2pi:
	set	r4,r4,1<le>		/* set le bit */
	set	r4,r4,1<lt>		/* set lt bit */
	bsr.n	_ASM_LABEL(zero)
	 set	r4,r4,1<ib>		/* set ib bit */
	bb0	s1zero,r12,FCMPS1pfS2pinozero
FCMPS1pfS2pizero:
	br.n	move
	 set	r4,r4,1<ob>		/* set ob bit */
FCMPS1pfS2pinozero:
	br.n	move
	 set	r4,r4,1<in>		/* set in bit */
FCMPS1pfS2ni:
	set	r4,r4,1<gt>		/* set gt bit */
	br.n	move
	 set	r4,r4,1<ge>		/* set ge bit */
FCMPS1nf:
	bb1.n	sign,r7,FCMPS1nfS2ni	/* branch to negative S2i with S1nf */
	 set	r4,r4,1<ne>		/* set ne bit */
	set	r4,r4,1<le>		/* set gt bit */
	set	r4,r4,1<lt>		/* set ge bit */
	bsr.n	_ASM_LABEL(zero)	/* see which of the operands are zero */
	 set	r4,r4,1<ob>		/* set ob bit */
	bb0	s1zero,r12,FCMPS1nfS2pinozero /* no ls and lo */
FCMPS1nfS2pizero:
	br.n	move
	 set	r4,r4,1<ib>		/* set ib bit */
FCMPS1nfS2pinozero:
	br.n	move
	 set	r4,r4,1<ou>		/* set ou bit */
FCMPS1nfS2ni:
	set	r4,r4,1<gt>		/* set gt bit */
	set	r4,r4,1<ge>		/* set ge bit */

move:
	br.n	inf_return
	 or	r6,r0,r4		/* transfer answer to r6 */


/* Multiplying infinity and zero causes an exception, but all other */
/* operations produce a correctly signed infinity. */

FMUL:
	bsr	_ASM_LABEL(zero)	/* see if any of the operands are zero */
	bb1	s1zero,r12,excpt	/* infinity X 0 causes an exception */
	bb1	s2zero,r12,excpt	/* infinity X 0 causes an exception */
	bb1	sign,r5,FMULS1neg	/* handle negative cases of S1 */
	bb0	sign,r7,poswrinf	/* + X + = + */
	br	negwrinf		/* + X - = - */
FMULS1neg:
	bb1	sign,r7,poswrinf	/* - X - = + */
	br	negwrinf		/* - X + = - */


/* Dividing infinity by infinity causes an exception, but dividing */
/* infinity by a finite yields a correctly signed infinity, and */
/* dividing a finite by an infinity produces a correctly signed zero. */

FDIV:
	bb1	s1inf,r12,FDIVS1inf	/* handle case of S1 being infinity */
	bb1	sign,r5,FDIVS1nf	/* handle cases of S1 being neg. non-inf. */
	bb1	sign,r7,FDIVS1pfS2mi	/* handle case of negative S2 */
FDIVS1pfS2pi:
	br	poswrzero		/* +f / +inf = +0 */
FDIVS1pfS2mi:
	br	negwrzero		/* +f / -inf = -0 */
FDIVS1nf:
	bb1	sign,r7,FDIVS1nfS2mi	/* handle case of negative S2 */
FDIVS1nfS2pi:
	br	negwrzero		/* -f / +inf = -0 */
FDIVS1nfS2mi:
	br	poswrzero		/* -f / -inf = +0 */
FDIVS1inf:
	bb1	s2inf,r12,excpt		/* inf / inf = exception */
	bb1	sign,r5,FDIVS1mi	/* handle cases of S1 being neg. inf. */
	bb1	sign,r7,FDIVS1piS2nf	/* handle case of negative S2 */
FDIVS1piS2pf:
	br	poswrinf		/* +inf / +f = +inf */
FDIVS1piS2nf:
	br	negwrinf		/* +inf / -f = -inf */
FDIVS1mi:
	bb1	sign,r7,FDIVS1miS2nf	/* handle case of negative S2 */
FDIVS1miS2pf:
	br	negwrinf		/* -inf / +f = -inf */
FDIVS1miS2nf:
	br	poswrinf		/* -inf / -f = +inf */


/* The square root of positive infinity is positive infinity, */
/* but the square root of negative infinity is a NaN */

#if 0
FSQRT:
	bb0	sign,r7,poswrinf	/* write sqrt(inf) = inf */
	br	excpt			/* write sqrt(-inf) = NaN */
#endif

excpt:
	set	r2,r2,1<oper>
#ifdef HANDLER
	bb0	oper,r3,nohandler	/* branch if no user handler */
	bsr	_handler		/* branch to interface with user handler */
	br	inf_return
nohandler:
#endif
	set	r5,r0,0<0>	/* write NaN into r5 */
	br.n	inf_return
	 set	r6,r0,0<0>	/* write NaN into r6, writing NaN''s into */
				/* both of these registers is quicker than */
				/* checking for single or double precision */


/* Write positive infinity of the correct precision */

poswrinf:
	bb1	dsize,r9,poswrinfd	/* branch to write double precision inf. */
	br.n	inf_return
	 or.u	r6,r0,0x7f80		/* load r6 with single precision pos inf.	*/
poswrinfd:
	or.u	r5,r0,0x7ff0		/* load double precision pos inf. */
	br.n	inf_return
	 or	r6,r0,r0


/* Write negative infinity of the correct precision */

negwrinf:
	bb1	dsize,r9,negwrinfd	/* branch to write double precision inf. */
	br.n	inf_return
	 or.u	r6,r0,0xff80		/* load r6 with single precision pos inf.	*/
negwrinfd:
	or.u	r5,r0,0xfff0		/* load double precision pos inf. */
	br.n	inf_return
	 or	r6,r0,r0


/* Write a positive zero disregarding precision. */

poswrzero:
	or	r5,r0,r0	/* write to both high word and low word now */
	br.n	inf_return	/* it does not matter that both are written */
	 or	r6,r0,r0


/* Write a negative zero of the correct precision. */

negwrzero:
	or	r6,r0,r0	/* clear low word */
	bb1	dsize,r9,negwrzerod /* branch to write double precision zero */
	br.n	inf_return
	 set	r6,r6,1<31>	/* set sign bit */
negwrzerod:
	or	r5,r0,r0	/* clear high word */
	br.n	inf_return
	 set	r5,r5,1<31>	/* set sign bit */

FP_inf_overflw:
	set	r2,r2,1<oper>
	set	r2,r2,1<overflow>
	set	r2,r2,1<inexact>
#ifdef HANDLER
	bb0	oper,r3,nohandlero /* do not go to user handler routine */
	bsr	_handler	/* go to user handler routine */
	br	inf_return
#endif

nohandlero:
	bb0.n	sign,r7,inf_return /* if positive then return */

	 set	r6,r6,31<0>	/* set result to largest positive integer */
	or.c	r6,r0,r6	/* negate r6,giving largest negative int. */

inf_return:
	ld	r1,r31,0	/* load return address */
	jmp	r1

	data

#define FADD	denorm_FADD
#define FSUB	denorm_FSUB
#define FCMP	denorm_FCMP
#define FMUL	denorm_FMUL
#define FDIV	denorm_FDIV
#define NINT	denorm_NINT
#define TRNC	denorm_TRNC
#define return	denorm_return

/*
 * denorm
 */

/* Check to see if either S1 or S2 is a denormalized number. First */
/* extract the exponent to see if it is zero, and then check to see if */
/* the mantissa is not zero. If the number is denormalized, then set the */
/* 1 or 0 bit 10 r12. */

ASLOCAL(denorm)
	st	r1,r31,0	/* save return address */
dnmcheckS1:
	extu	r10,r5,11<20>	/* extract exponent */
	bcnd	ne0,r10,dnmsetS2 /* S1 is not a denorm, so S2 must be */
	bb1.n	9,r9,dnmcheckS1d /* S1 is double precision */
	 mak	r10,r5,20<3>	/* mak field with only mantissa bits */
				/* into final result */
dnmcheckS1s:
	extu	r11,r6,3<29>	/* get three low bits of mantissa */
	or	r10,r10,r11	/* assemble all of the mantissa bits */
	bcnd	eq0,r10,dnmsetS2 /* S1 is not a denorm, so S2 must be */
	br	dnmsetS1	/* S1 is a denorm */

dnmcheckS1d:
	or	r10,r6,r10	/* or all of mantissa bits */
	bcnd	eq0,r10,dnmsetS2 /* S1 is not a denorm, so S2 must be */
dnmsetS1:
	set	r12,r12,1<1>	/* S1 is a denorm */

dnmcheckS2:
	extu	r10,r7,11<20>	/* extract exponent */
	bcnd	ne0,r10,S1form	/* S2 is not a denorm */
	bb1.n	7,r9,dnmcheckS2d /* S2 is double precision */
	 mak	r10,r7,20<3>	/* mak field with only mantissa bits */
dnmcheckS2s:
	extu	r11,r8,3<29>	/* get three low bits of mantissa */
	or	r10,r10,r11	/* assemble all of the mantissa bits */
	bcnd	eq0,r10,S1form	/* S2 is not a denorm */
	br	dnmsetS2	/* S1 is a denorm */
dnmcheckS2d:
	or	r10,r8,r10	/* or all or mantissa bits */
	bcnd	eq0,r10,S1form	/* S2 is not a denorm */
dnmsetS2:
	set	r12,r12,1<0>	/* S2 is a denorm */


/* Since the operations are going to be reperformed with modified denorms, */
/* the operands which were initially single precision need to be modified */
/* back to single precision.	*/

S1form:
	bb1	9,r9,S2form	/* S1 is double precision, so do not */
				/* modify S1 into single format */
	mak	r11,r5,28<3>	/* over final exponent and mantissa */
				/* eliminating extra 3 bits of exponent */
	extu	r6,r6,3<29>	/* get low 3 bits of mantissa */
	or	r11,r6,r11	/* form complete mantissa and exponent */
	extu	r10,r5,1<31>	/* get the 31 bit */
	mak	r10,r10,1<31>	/* place 31 bit 10 correct position */
	or	r6,r10,r11	/* or 31, exponent, and all of mantissa */

S2form:
	bb1	7,r9,checkop	/* S2 is double precision, so do not */
				/* modify S2 into single format */
	mak	r11,r7,28<3>	/* over final exponent and mantissa */
				/* eliminating extra 3 bits of exponent */
	extu	r8,r8,3<29>	/* get low 3 bits of mantissa */
	or	r11,r8,r11	/* form complete mantissa and exponent */
	extu	r10,r7,1<31>	/* get the 31 bit */
	mak	r10,r10,1<31>	/* place 31 bit 10 correct position */
	or	r8,r10,r11	/* or 31, exponent, and all of mantissa */


/* Extract the opcode, compare to a constant, and branch to the code that */
/* deals with that opcode. */

checkop:
	extu	r10,r9,5<11>	/* extract opcode */
	cmp	r11,r10,0x05	/* compare to FADD */
	bb1	2,r11,FADD	/* operation is FADD */
	cmp	r11,r10,0x06	/* compare to FSUB */
	bb1	2,r11,FSUB	/* operation is FSUB */
	cmp	r11,r10,0x07	/* compare to FCMP */
	bb1	2,r11,FCMP	/* operation is FCMP */
	cmp	r11,r10,0x00	/* compare to FMUL */
	bb1	2,r11,FMUL	/* operation is FMUL */
	cmp	r11,r10,0x0e	/* compare to FDIV */
	bb1	2,r11,FDIV	/* operation is FDIV */
#if 0
	cmp	r11,r10,0x0f	/* compare to FSQRT */
	bb1	2,r11,FSQRT	/* operation is FSQRT */
#endif
	cmp	r11,r10,0x09	/* compare to INT */
	bb1	2,r11,INT	/* operation is INT */
	cmp	r11,r10,0x0a	/* compare to NINT */
	bb1	2,r11,NINT	/* operation is NINT */
	cmp	r11,r10,0x0b	/* compare to TRNC */
	bb1	2,r11,TRNC	/* operation is TRNC */


/* For all the following operations, the denormalized number is set to */
/* zero and the operation is reperformed the correct destination and source */
/* sizes. */

FADD:
	bb0	1,r12,FADDS2dnm	/* S1 is not denorm, so S2 must be */
	or	r5,r0,r0	/* set S1 to zero */
	or	r6,r0,r0
FADDS2chk:
	bb0	0,r12,FADDcalc	/* S2 is not a denorm */
FADDS2dnm:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FADDcalc:
	bb1	5,r9,FADDdD	/* branch for double precision destination */
FADDsD:
	bb1	9,r9,FADDsDdS1	/* branch for double precision S1 */
FADDsDsS1:
	bb1	7,r9,FADDsDsS1dS2 /* branch for double precision S2 */
FADDsDsS1sS2:
	br.n	return
	 fadd.sss r6,r6,r8	/* add the two sources and place result 10 S1 */
FADDsDsS1dS2:
	br.n	return
	 fadd.ssd r6,r6,r7	/* add the two sources and place result 10 S1 */
FADDsDdS1:
	bb1	7,r9,FADDsDdS1dS2 /* branch for double precision S2 */
FADDsDdS1sS2:
	br.n	return
	 fadd.sds r6,r5,r8	/* add the two sources and place result 10 S1 */
FADDsDdS1dS2:
	br.n	return
	 fadd.sdd r6,r5,r7	/* add the two sources and place result 10 S1 */
FADDdD:
	bb1	9,r9,FADDdDdS1	/* branch for double precision S1 */
FADDdDsS1:
	bb1	7,r9,FADDdDsS1dS2 /* branch for double precision S2 */
FADDdDsS1sS2:
	br.n	return
	 fadd.dss r5,r6,r8	/* add the two sources and place result 10 S1 */
FADDdDsS1dS2:
	br.n	return
	 fadd.dsd r5,r6,r7	/* add the two sources and place result 10 S1 */
FADDdDdS1:
	bb1	7,r9,FADDdDdS1dS2 /* branch for double precision S2 */
FADDdDdS1sS2:
	br.n	return
	 fadd.dds r5,r5,r8	/* add the two sources and place result 10 S1 */
FADDdDdS1dS2:
	br.n	return
	 fadd.ddd r5,r5,r7	/* add the two sources and place result 10 S1 */

FSUB:
	bb0	1,r12,FSUBS2dnm	/* S1 is not denorm, so S2 must be */
	or	r5,r0,r0	/* set S1 to zero */
	or	r6,r0,r0
FSUBS2chk:
	bb0	0,r12,FSUBcalc	/* S2 is not a denorm */
FSUBS2dnm:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FSUBcalc:
	bb1	5,r9,FSUBdD	/* branch for double precision destination */
FSUBsD:
	bb1	9,r9,FSUBsDdS1	/* branch for double precision S1 */
FSUBsDsS1:
	bb1	7,r9,FSUBsDsS1dS2 /* branch for double precision S2 */
FSUBsDsS1sS2:
	br.n	return
	 fsub.sss r6,r6,r8	/* add the two sources and place result 10 S1 */
FSUBsDsS1dS2:
	br.n	return
	 fsub.ssd r6,r6,r7	/* add the two sources and place result 10 S1 */
FSUBsDdS1:
	bb1	7,r9,FSUBsDdS1dS2 /* branch for double precision S2 */
FSUBsDdS1sS2:
	br.n	return
	 fsub.sds r6,r5,r8	/* add the two sources and place result 10 S1 */
FSUBsDdS1dS2:
	br.n	return
	 fsub.sdd r6,r5,r7	/* add the two sources and place result 10 S1 */
FSUBdD:
	bb1	9,r9,FSUBdDdS1	/* branch for double precision S1 */
FSUBdDsS1:
	bb1	7,r9,FSUBdDsS1dS2 /* branch for double precision S2 */
FSUBdDsS1sS2:
	br.n	return
	 fsub.dss r5,r6,r8	/* add the two sources and place result 10 S1 */
FSUBdDsS1dS2:
	br.n	return
	 fsub.dsd r5,r6,r7	/* add the two sources and place result 10 S1 */
FSUBdDdS1:
	bb1	7,r9,FSUBdDdS1dS2 /* branch for double precision S2 */
FSUBdDdS1sS2:
	br.n	return
	 fsub.dds r5,r5,r8	/* add the two sources and place result 10 S1 */
FSUBdDdS1dS2:
	br.n	return
	 fsub.ddd r5,r5,r7	/* add the two sources and place result 10 S1 */

FCMP:
	bb0	1,r12,FCMPS2dnm	/* S1 is not denorm, so S2 must be */
	or	r5,r0,r0	/* set S1 to zero */
	or	r6,r0,r0
FCMPS2chk:
	bb0	0,r12,FCMPcalc	/* S2 is not a denorm */
FCMPS2dnm:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FCMPcalc:
	bb1	9,r9,FCMPdS1	/* branch for double precision S1 */
FCMPsS1:
	bb1	7,r9,FCMPsS1dS2	/* branch for double precision S2 */
FCMPsS1sS2:
	br.n	return
	 fcmp.sss r6,r6,r8	/* add the two sources and place result 10 S1 */
FCMPsS1dS2:
	br.n	return
	 fcmp.ssd r6,r6,r7	/* add the two sources and place result 10 S1 */
FCMPdS1:
	bb1	7,r9,FCMPdS1dS2	/* branch for double precision S2 */
FCMPdS1sS2:
	br.n	return
	 fcmp.sds r6,r5,r8	/* add the two sources and place result 10 S1 */
FCMPdS1dS2:
	br.n	return
	 fcmp.sdd r6,r5,r7	/* add the two sources and place result 10 S1 */

FMUL:
	bb0	1,r12,FMULS2dnm	/* S1 is not denorm, so S2 must be */
	or	r5,r0,r0	/* set S1 to zero */
	or	r6,r0,r0
FMULS2chk:
	bb0	0,r12,FMULcalc	/* S2 is not a denorm */
FMULS2dnm:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FMULcalc:
	bb1	5,r9,FMULdD	/* branch for double precision destination */
FMULsD:
	bb1	9,r9,FMULsDdS1	/* branch for double precision S1 */
FMULsDsS1:
	bb1	7,r9,FMULsDsS1dS2 /* branch for double precision S2 */
FMULsDsS1sS2:
	br.n	return
	 fmul.sss r6,r6,r8	/* add the two sources and place result 10 S1 */
FMULsDsS1dS2:
	br.n	return
	 fmul.ssd r6,r6,r7	/* add the two sources and place result 10 S1 */
FMULsDdS1:
	bb1	7,r9,FMULsDdS1dS2 /* branch for double precision S2 */
FMULsDdS1sS2:
	br.n	return
	 fmul.sds r6,r5,r8	/* add the two sources and place result 10 S1 */
FMULsDdS1dS2:
	br.n	return
	 fmul.sdd r6,r5,r7	/* add the two sources and place result 10 S1 */
FMULdD:
	bb1	9,r9,FMULdDdS1	/* branch for double precision S1 */
FMULdDsS1:
	bb1	7,r9,FMULdDsS1dS2 /* branch for double precision S2 */
FMULdDsS1sS2:
	br.n	return
	 fmul.dss r5,r6,r8	/* add the two sources and place result 10 S1 */
FMULdDsS1dS2:
	br.n	return
	 fmul.dsd r5,r6,r7	/* add the two sources and place result 10 S1 */
FMULdDdS1:
	bb1	7,r9,FMULdDdS1dS2 /* branch for double precision S2 */
FMULdDdS1sS2:
	br.n	return
	 fmul.dds r5,r5,r8	/* add the two sources and place result 10 S1 */
FMULdDdS1dS2:
	br.n	return
	 fmul.ddd r5,r5,r7	/* add the two sources and place result 10 S1 */

FDIV:
	bb0	1,r12,FDIVS2dnm	/* S1 is not denorm, so S2 must be */
	or	r5,r0,r0	/* set S1 to zero */
	or	r6,r0,r0
FDIVS2chk:
	bb0	0,r12,FDIVcalc	/* S2 is not a denorm */
FDIVS2dnm:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FDIVcalc:
	bb1	5,r9,FDIVdD	/* branch for double precision destination */
FDIVsD:
	bb1	9,r9,FDIVsDdS1	/* branch for double precision S1 */
FDIVsDsS1:
	bb1	7,r9,FDIVsDsS1dS2 /* branch for double precision S2 */
FDIVsDsS1sS2:
	fdiv.sss r6,r6,r8	/* add the two sources and place result 10 S1 */
	br	return
FDIVsDsS1dS2:
	fdiv.ssd r6,r6,r7	/* add the two sources and place result 10 S1 */
	br	return
FDIVsDdS1:
	bb1	7,r9,FDIVsDdS1dS2 /* branch for double precision S2 */
FDIVsDdS1sS2:
	fdiv.sds r6,r5,r8	/* add the two sources and place result 10 S1 */
	br	return
FDIVsDdS1dS2:
	fdiv.sdd r6,r5,r7	/* add the two sources and place result 10 S1 */
	br	return
FDIVdD:
	bb1	9,r9,FDIVdDdS1	/* branch for double precision S1 */
FDIVdDsS1:
	bb1	7,r9,FDIVdDsS1dS2 /* branch for double precision S2 */
FDIVdDsS1sS2:
	fdiv.dss r5,r6,r8	/* add the two sources and place result 10 S1 */
	br	return
FDIVdDsS1dS2:
	fdiv.dsd r5,r6,r7	/* add the two sources and place result 10 S1 */
	br	return
FDIVdDdS1:
	bb1	7,r9,FDIVdDdS1dS2 /* branch for double precision S2 */
FDIVdDdS1sS2:
	fdiv.dds r5,r5,r8	/* add the two sources and place result 10 S1 */
	br	return
FDIVdDdS1dS2:
	fdiv.ddd r5,r5,r7	/* add the two sources and place result 10 S1 */
	br	return

#if 0
FSQRT:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
FSQRTcalc:
	bb1	5,r9,FSQRTdD	/* branch for double precision destination */
FSQRTsD:
	bb1	7,r9,FSQRTsDdS2 /* branch for double precision S2 */
FSQRTsDsS2:
	br.n	return
	 fsqrt.ss r6,r8		/* add the two sources and place result 10 S1 */
FSQRTsDdS2:
	br.n	return
	 fsqrt.sd r6,r7		/* add the two sources and place result 10 S1 */
FSQRTdD:
	bb1	7,r9,FSQRTdDdS2	/* branch for double precision S2 */
FSQRTdDsS2:
	br.n	return
	 fsqrt.ds r5,r8		/* add the two sources and place result 10 S1 */
FSQRTdDdS2:
	br.n	return
	 fsqrt.dd r5,r7		/* add the two sources and place result 10 S1 */
#endif

INT:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
INTcalc:
	bb1	7,r9,INTdS2	/* branch for double precision S2 */
INTsS2:
	br.n	return
	 int.ss r6,r8		/* add the two sources and place result 10 S1 */
INTdS2:
	br.n	return
	 int.sd r6,r7		/* add the two sources and place result 10 S1 */

NINT:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
NINTcalc:
	bb1	7,r9,NINTdS2	/* branch for double precision S2 */
NINTsS2:
	br.n	return
	 nint.ss r6,r8		/* add the two sources and place result 10 S1 */
NINTdS2:
	br.n	return
	 nint.sd r6,r7		/* add the two sources and place result 10 S1 */

TRNC:
	or	r7,r0,r0	/* set S2 to zero */
	or	r8,r0,r0
TRNCcalc:
	bb1	7,r9,TRNCdS2	/* branch for double precision S2 */
TRNCsS2:
	br.n	return
	 trnc.ss r6,r8		/* add the two sources and place result 10 S1 */
TRNCdS2:
	trnc.sd r6,r7		/* add the two sources and place result 10 S1 */


/* Return to the routine that detected the reserved operand. */

return:
	ld	r1,r31,0	/* load return address */
	jmp	r1

	data


/* S1 and/or S2 is an infinity, and the other operand may be a zero. */
/* Knowing which operands are infinity, check the remaining operands for zeros. */

ASLOCAL(zero)
	bb0	s1inf,r12,S1noinf	/* see if S1 is zero */
	bb0	s2inf,r12,S2noinf	/* see if S2 is zero */
	jmp	r1

/* See if S1 is zero. Whether or not S1 is a zero, being in this routine */
/* implies that S2 is infinity, so return to subroutine infinity after */
/* completing this code. Set the s1zero flag in r12 if S1 is zero. */

S1noinf:
	bb1	s1size,r9,S1noinfd	/* work with double precision operand */
S1noinfs:
	or	r10,r0,r5		/* load high word into r10 */
	clr	r10,r10,1<sign>		/* clear the sign bit */
	extu	r11,r6,3<29>		/* extract lower 3 bits of mantissa */
	or	r10,r10,r11		/* or these 3 bits with high word */
	bcnd	ne0,r10,operation	/* do not set zero flag */
	jmp.n	r1			/* since this operand was not */
					/* infinity, S2 must have been, */
					/* so return */
	 set	r12,r12,1<s1zero>	/* set zeroflag */
S1noinfd:
	clr	r10,r5,1<sign>		/* clear the sign bit */
	or	r10,r6,r10		/* or high and low word */
	bcnd	ne0,r10,operation	/* do not set zero flag */
	jmp.n	r1			/* since this operand was not */
					/* infinity, S2 must have been, */
					/* so return */
	 set	r12,r12,1<s1zero>	/* set zeroflag */


/* Check S2 for zero. If it is zero, then set the s2zero flag in r12. */

S2noinf:
	bb1	s2size,r9,S2noinfd	/* work with double precision operand */
S2noinfs:
	or	r10,r0,r7		/* load high word into r10 */
	clr	r10,r10,1<sign>		/* clear the sign bit */
	extu	r11,r8,3<29>		/* extract lower 3 bits of mantissa */
	or	r10,r10,r11		/* or these 3 bits with high word */
	bcnd	ne0,r10,operation	/* do not set zero flag */
	jmp.n	r1			/* since this operand was not */
					/* infinity, S1 must have been, */
					/* so return */
	 set	r12,r12,1<s2zero>	/* set zeroflag */
S2noinfd:
	clr	r10,r7,1<sign>		/* clear the sign bit */
	or	r10,r8,r10		/* or high and low word */
	bcnd	ne0,r10,operation	/* do not set zero flag */
	set	r12,r12,1<s2zero>	/* set zeroflag */
					/* since this operand was not */
					/* infinity, S1 must have been, */
					/* so return */
operation:
	jmp	r1

ASENTRY(Xfp_imprecise)
/* input: r3 is the exception frame */
	or	r29, r3, r0		/* r29 is now the E.F. */
	subu	r31, r31, 40
	st	r1,  r31, 32
	st	r29, r31, 36

	ld	r2 , r29, EF_FPSR  * 4
	ld	r3 , r29, EF_FPCR  * 4
	ld	r4 , r29, EF_FPECR * 4
	ld	r10, r29, EF_FPRH  * 4
	ld	r11, r29, EF_FPRL  * 4
	ld	r12, r29, EF_FPIT  * 4

/* Load into r1 the return address for the exception handlers. Looking */
/* at FPECR, branch to the appropriate exception handler. */

	or.u	r1,r0,hi16(fpui_wrapup)/* load return address of functions */
	or	r1,r1,lo16(fpui_wrapup)

	bb0	2,r4,2f			/* branch to FPunderflow if bit set */
	br	_ASM_LABEL(FPunderflow)
2:
	bb0	1,r4,3f			/* branch to FPoverflow if bit set */
	br	_ASM_LABEL(FPoverflow)
3:
	/* XXX handle inexact!!! */
#ifdef HANDLER
	br	_handler		/* branch to handler since bit will */
					/* be set for inexact */
#endif

fpui_wrapup:
	tb1	0,r0,0		/* make sure all floating point operations */
				/* have finished */
	ldcr	r4, cr1	/* load the PSR */
#if 0
	set	r4, r4, 1<PSR_FPU_DISABLE_BIT>
#endif
	set	r4, r4, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r4, cr1
	ld	r1, r31, 32
	ld	r29,r31, 36
	addu	r31, r31, 40

	fstcr	r2, FPSR	/* write revised value of FPSR */
	fstcr	r3, FPCR	/* write revised value of FPCR */

	/* write back the results */
	extu	r2, r12, 5<0>
	addu	r3, r29, EF_R0*4
	bb0	destsize, r12, Iwritesingle
	st	r10, r3 [r2]
	addu	r2, r2, 1
	clr	r2, r2, 27<5>
Iwritesingle:
	st	r11, r3 [r2]
	jmp	r1
