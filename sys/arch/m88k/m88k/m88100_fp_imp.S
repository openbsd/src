/* $OpenBSD: m88100_fp_imp.S,v 1.1 2014/06/09 10:26:10 miod Exp $	*/
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
#include <machine/asm.h>

#define destsize 10

/* Floating-Point Status Register bits */
#define inexact		0
#define overflow	1
#define underflow	2

#define sign		31

#define modehi		30
#define modelo		29

/*
 * Branch to the routine to make a denormalized number.
 */
ASLOCAL(FPunderflow)
	st	%r1, %r31, 0	/* save return address */
	set	%r2, %r2,  1<underflow>
	set	%r2, %r2,  1<inexact>

/*
 * Now the floating point number, which has an exponent smaller than what
 * IEEE allows, must be denormalized. Denormalization is done by calculating
 * the difference between a denormalized exponent and an underflow exponent
 * and shifting the mantissa by that amount. A one may need to be subtracted
 * from the LSB if a one was added during rounding.
 * %r9 is used to contain the guard, round, sticky, and an inaccuracy bit in
 * case some bits were shifted off the mantissa during denormalization.
 * %r9 will contain:
 *   bit 4 -- new addone if one added during rounding after denormalization
 *   bit 3 -- inaccuracy flag caused by denormalization or pre-denormalization
 *            inexactness
 *   bit 2 -- guard bit of result
 *   bit 1 -- round bit of result
 *   bit 0 -- sticky bit of result
 */

FPU_denorm:
	bb1.n	destsize, %r12, Udouble	/* denorm for double */
	 extu	%r9,  %r10, 3<26>	/* load r9 with grs */
Usingle:
	mak	%r5,  %r10, 21<3>	/* extract high 21 bits of mantissa */
	extu	%r6,  %r11, 3<29>	/* extract low 3 bits of mantissa */
	or	%r11, %r5,  %r6		/* form 24 bits of mantissa */

/* See if the addone bit is set and unround if it is. */
	bb0.n	25,   %r10, nounrounds	/* do not unround if addone bit clear */
	 extu	%r6,  %r12, 12<20>	/* extract signed exponent from IMPCR */
unrounds:
	subu	%r11, %r11, 1		/* subtract 1 from mantissa */

/*
 * If the hidden bit is cleared after subtracting the one, then the one added
 * during the rounding must have propagated through the mantissa. The exponent
 * will need to be decremented.
 */
	bb1	23,   %r11, nounrounds	/* if hidden bit is set, the exponent */
					/* does not need to be decremented */
decexps:
	sub	%r6,  %r6,  1		/* decrement exponent */
	set	%r11, %r11, 1<23>	/* set the hidden bit */

/*
 * For both single and double precision, there are cases where it is easier
 * and quicker to make a special case. Examples of this are if the shift
 * amount is only 1 or 2, or all the mantissa is shifted off, or all the
 * mantissa is shifted off and it is still shifting, or, in the case of
 * doubles, if the shift amount is around the boundary of MANTLO and MANTHI.
 */

nounrounds:
	or	%r8,  %r0,  %lo16(0x00000f81)
					/* load r8 with -127 in decimal */
					/* for lowest 12 bits */
	sub	%r7,  %r8,  %r6		/* find difference between two */
					/* exponents, this amount is the */
					/* shift amount */
	cmp	%r6,  %r7,  3		/* check to see if r7 contains 3 */
					/* or more */
	bb1	ge,   %r6,  threesing	/* br to code that handles shifts of */
					/* 3 or more */
	cmp	%r6,  %r7,  2		/* check to see if r7 contains 2 */
	bb1	eq,   %r6,  twosing	/* br to code that handles shifts of */
					/* 2 */
one:
	rot	%r9,  %r9,  0<1>	/* rotate roundoff register once, */
					/* this places guard in round and */
					/* round in sticky */
	bb0	31,   %r9,  nosticky1s	/* do not or round and sticky if */
					/* sticky is 0, this lost bit will */
					/* be cleared later */
	set	%r9,  %r9,  1<0>	/* or round and sticky */
nosticky1s:
	bb0	0,    %r11, guardclr1s	/* do not set guard bit if LSB = 0 */
	set	%r9,  %r9,  1<2>	/* set guard bit */
guardclr1s:
	extu	%r11, %r11, 31<1>	/* shift mantissa right 1 */
	br.n	round			/* round result */
	 mak	%r9,  %r9,  3<0>	/* clear bits lost during rotation */

twosing:
	rot	%r9,  %r9,  0<2>	/* rotate roundff register twice, */
					/* this places guard in sticky */
	bb0	30,   %r9,  nosticky2s	/* do not or guard and sticky if */
					/* sticky is 0, this lost bit will be */
					/* cleared later */
	br.n	noround2s		/* skip or old guard and old round if */
					/* old sticky set */
	 set	%r9,  %r9,  1<0>	/* or guard and sticky */
nosticky2s:
	bb0	31,   %r9,  noround2s	/* do not or guard and round if round */
					/* is 0, this lost bit will be */
					/* cleared later */
	set	%r9,  %r9,  1<0>	/* or guard and round */
noround2s:
	bb0	0,    %r11, roundclr2s	/* do not set round bit if LSB = 0 */
	set	%r9,  %r9,  1<1>	/* set round bit */
roundclr2s:
	bb0	1,    %r11, guardclr2s	/* do not set guard bit if LSB+1 = 0 */
	set	%r9,  %r9,  1<2>	/* set guard bit */
guardclr2s:
	extu	%r11, %r11, 30<2>	/* shift mantissa right 2 */
	br.n	round			/* round result */
	 mak	%r9,  %r9,  3<0>	/* clear bits lost during rotation */

threesing:
	bb1	0,    %r9,  noguard3s	/* check sticky initially */
					/* sticky is set, forget most of the */
					/* oring */
nosticky3s:
	bb0	1,    %r9,  noround3s	/* check round initially, do not set */
					/* sticky */
	br.n	noguard3s		/* forget most of the rest of oring */
	 set	%r9,  %r9,  1<0>	/* if round is clear, set sticky if */
					/* round set */
noround3s:
	bb0.n	2,    %r9,  noguard3s	/* check guard initially, do not set */
					/* sticky */
	 clr	%r9,  %r9,  2<1>	/* clear the original guard and round */
					/* for when you get to round section */
	set	%r9,  %r9,  1<0>	/* if guard is clear, set sticky if */
					/* guard set */
noguard3s:
	cmp	%r6,  %r7,  23		/* check if # of shifts is <=23 */
	bb1	gt,   %r6,  s24		/* branch to see if shifts = 24 */
	sub	%r6,  %r7,  2		/* get number of bits to check for */
					/* sticky */
	mak	%r6,  %r6,  5<5>	/* shift width into width field */
	mak	%r8,  %r11, %r6		/* mask off shifted bits -2 */
	ff1	%r8,  %r8		/* see if r8 has any ones */
	bb1	5,    %r8,  nostky23	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky23:
	or	%r8,  %r0,  0x22	/* start code to get new mantissa */
					/* plus two extra bits for new round */
					/* and new guard bits */
	subu	%r8,  %r8,  %r7
	mak	%r8,  %r8,  5<5>	/* shift field width into second five */
					/* bits */
	extu	%r6,  %r6,  5<5>	/* shift previous shifted -2 into */
					/* offset field */
	or	%r6,  %r6,  %r8		/* complete field */
	extu	%r11, %r11, %r6		/* form new mantissa with two extra */
					/* bits */

	bb0	0,    %r11, nornd3s	/* do not set new round bit */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd3s:
	bb0	1,    %r11, nogrd3s	/* do not set new guard bit */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
nogrd3s:
	br.n	round			/* round mantissa */
	 extu	%r11, %r11, 30<2>	/* shift off remaining two bits */

s24:
	cmp	%r6,  %r7,  24		/* check to see if # of shifts is 24 */
	bb1	gt,   %r6,  s25		/* branch to see if shifts = 25 */
	bb1	0,    %r9,  nostky24	/* skip checking if old sticky set */
	extu	%r8,  %r11, 22<0>	/* prepare to check bits that will be */
					/* shifted into the sticky */
	ff1	%r8,  %r8		/* see if there are any 1''s */
	bb1	5,    %r8,  nostky24	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky24:
	bb0	22,   %r11, nornd24	/* do not set new round bit */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd24:
	set	%r9,  %r9,  1<2>	/* set new guard bit, this is hidden */
					/* bit */
	br.n	round			/* round mantissa */
	 or	%r11, %r0,  %r0		/* clear r11, all of mantissa */
					/* shifted off */

s25:
	cmp	%r6,  %r7,  25		/* check to see if # of shifts is 25 */
	bb1	gt,   %r6,  s26		/* branch to execute for shifts => 26 */
	bb1	0,    %r9,  nostky25	/* skip checking if old sticky set */
	extu	%r8,  %r11, 23<0>	/* prepare to check bits that will be */
					/* shifted into the sticky */
	ff1	%r8,  %r8		/* see if there are any 1''s */
	bb1	5,    %r8,  nostky25	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky25:
	set	%r9,  %r9,  1<1>	/* set new round bit, this is hidden */
					/* bit */
	clr	%r9,  %r9,  1<2>	/* clear guard bit since nothing */
					/* shifted in */
	br.n	round			/* round and assemble result */
	 or	%r11, %r0,  %r0		/* clear r11, all of mantissa */
					/* shifted off */

s26:
	set	%r9,  %r9,  1<0>	/* set sticky bit, this contains */
					/* hidden bit */
	clr	%r9,  %r9,  2<1>	/* clear guard and round bits since */
					/* nothing shifted in */
	br.n	round			/* round and assemble result */
	 or	%r11, %r0,  %r0		/* clear mantissa */

Udouble:
	mak	%r5,  %r10, 21<0>	/* extract upper bits of mantissa */
	bb0.n	25,   %r10, nounroundd	/* do not unround if addone bit clear */
	 extu	%r6,  %r12, 12<20>	/* extract signed exponenet from IMPCR */
unroundd:
	or	%r8,  %r0,  1
	subu.co	%r11, %r11, %r8		/* subtract 1 from mantissa */
	subu.ci	%r5,  %r5,  %r0		/* subtract borrow from upper word */
	bb1	20,   %r5,  nounroundd	/* if hidden bit is set, then */
					/* exponent does not need to be */
					/* decremented */
decexpd:
	sub	%r6,  %r6,  1		/* decrement exponent */
	set	%r5,  %r5,  1<20>	/* set the hidden bit */

nounroundd:
	or	%r8,  %r0,  %lo16(0x00000c01)
					/* load r8 with -1023 in decimal */
					/* for lowest 12 bits */
	sub	%r7,  %r8,  %r6		/* find difference between two */
					/* exponents, this amount is the */
					/* shift amount */
	cmp	%r6,  %r7,  3		/* check to see if r7 contains */
					/* 3 or more */
	bb1	ge,   %r6,  threedoub	/* br to code that handles shifts of */
					/* 3 or more */
	cmp	%r6,  %r7,  2		/* check to see if r7 contains 2 */
	bb1	eq,   %r6,  twodoub	/* br to code that handles shifts */
					/* of 2 */

onedoub:
	rot	%r9,  %r9,  0<1>	/* rotate roundoff register once, */
					/* this places guard in round and */
					/* round in sticky */
	bb0	31,   %r9,  nosticky1d	/* do not or round and sticky if */
					/* sticky is 0, this lost bit */
					/* will be cleared later */
	set	%r9,  %r9,  1<0>	/* or old round and old sticky into */
					/* new sticky */
nosticky1d:
	bb0	0,    %r11, guardclr1d	/* do not set new guard bit */
					/* if old LSB = 0 */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
guardclr1d:
	extu	%r11, %r11, 31<1>	/* shift lower mantissa over 1 */
	mak	%r6,  %r5,  1<31>	/* shift off low bit of high mantissa */
	or	%r11, %r6,  %r11	/* load high bit onto lower mantissa */
	extu	%r5,  %r5,  20<1>	/* shift right once upper 20 bits of */
					/* mantissa */
	br.n	round			/* round mantissa and assemble result */
	 mak	%r9,  %r9,  3<0>	/* clear bits lost during rotation */

twodoub:
	rot	%r9,  %r9,  0<2>	/* rotate roundoff register twice, */
					/* this places old guard into sticky */
	bb0	30,   %r9,  nosticky2d	/* do not or old guard and old sticky */
					/* if old sticky is 0 */
	br.n	noround2d		/* skip or of old guard and old round */
					/* if old sticky set */
	 set	%r9,  %r9,  1<0>	/* or old guard and old sticky into */
					/* new sticky */
nosticky2d:
	bb0	31,   %r9,  noround2d	/* do not or old guard and old round */
					/* if old round is 0 */
	set	%r9,  %r9,  1<0>	/* or old guard and old round into */
					/* new sticky */
noround2d:
	bb0	0,    %r11, roundclr2d	/* do not set round bit */
					/* if old LSB = 0 */
	set	%r9,  %r9,  1<1>	/* set new round bit */
roundclr2d:
	bb0	1,    %r11, guardclr2d	/* do not set guard bit */
					/* if old LSB + 1 = 0 */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
guardclr2d:
	extu	%r11, %r11, 30<2>	/* shift lower mantissa over 2 */
	mak	%r6,  %r5,  2<30>	/* shift off low bits of high */
					/* mantissa */
	or	%r11, %r6,  %r11	/* load high bit onto lower mantissa */
	extu	%r5,  %r5,  19<2>	/* shift right twice upper 19 bits of */
					/* mantissa */
	br.n	round			/* round mantissa and assemble result */
	 mak	%r9,  %r9,  3<0>	/* clear bits lost during rotation */

threedoub:
	bb1	0,    %r9,  noguard3d	/* checky sticky initially */
					/* sticky is set, forget most of */
					/* rest of oring */
nosticky3d:
	bb0	1,    %r9,  noround3d	/* check old round, do not set sticky */
					/* if old round is clear, */
					/* set otherwise */
	br.n	noguard3d		/* sticky is set, forget most of */
					/* rest of oring */
	 set	%r9,  %r9,  1<0>	/* set sticky if old round is set */
noround3d:
	bb0	2,    %r9,  noguard3d	/* check old guard, do not set sticky */
					/* if 0 */
	clr	%r9,  %r9,  2<1>	/* clear the original guard and round */
					/* for when you get to round section */
	set	%r9,  %r9,  1<0>	/* set sticky if old guard is set */
noguard3d:
	cmp	%r6,  %r7,  32		/* do I need to work with a 1 or 2 */
					/* word mantissa when forming sticky, */
					/* round and guard */
	bb1	gt,   %r6,  d33		/* jump to code that handles 2 word */
					/* mantissas */
	sub	%r6,  %r7,  2		/* get number of bits to check for */
					/* sticky */
	mak	%r6,  %r6,  5<5>	/* shift width into width field */
	mak	%r8,  %r11, %r6		/* mask off shifted bits -2 */
	ff1	%r8,  %r8		/* see if r8 has any ones */
	bb1	5,    %r8,  nostky32	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky32:
	or	%r8,  %r0,  0x22	/* start code to get new mantissa */
					/* plus two extra bits for new round */
					/* and new guard bits, the upper word */
					/* bits will be shifted after the */
					/* round and guard bits are handled */
	subu	%r8,  %r8,  %r7
	mak	%r8,  %r8,  5<5>	/* shift field width into second five */
					/* bits */
	extu	%r6,  %r6,  5<5>	/* shift previous shifted -2 into */
					/* offset field */
	or	%r6,  %r6,  %r8		/* complete bit field */
	extu	%r11, %r11, %r6		/* partially form new low mantissa */
					/* with 2 more bits */
	bb0	0,    %r11, nornd32d	/* do not set new round bit */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd32d:
	bb0	1,    %r11, nogrd32d	/* do not set new guard bit */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
nogrd32d:
	extu	%r11, %r11, 30<2>	/* shift off remaining two bits */
	mak	%r6,  %r7,  5<5>	/* shift field width into second 5 */
					/* bits, if the width is 32, then */
					/* these bits will be 0 */
	or	%r8,  %r0,  32		/* load word length into r8 */
	sub	%r8,  %r8,  %r7		/* form offset for high bits moved to */
					/* low word */
	or	%r6,  %r6,  %r8		/* form complete bit field */
	mak	%r6,  %r5,  %r6		/* get shifted bits of high word */
	or	%r11, %r6,  %r11	/* form new low word of mantissa */
	bcnd	ne0,  %r8,  regular33	/* do not adjust for special case */
					/* of r8 containing zeros, which */
	br.n	round			/* would cause all of the bits to be */
	 or	%r5,  %r0,  %r0		/* extracted under the regular method */
regular33:
	mak	%r6,  %r7,  5<0>	/* place lower 5 bits of shift */
					/* into r6 */
	mak	%r8,  %r8,  5<5>	/* shift r8 into width field */
	or	%r6,  %r6,  %r8		/* form field for shifting of upper */
					/* bits */
	br.n	round			/* round and assemble result */
	 extu	%r5,  %r5,  %r6		/* form new high word mantissa */

d33:
	cmp	%r6,  %r7,  33		/* is the number of bits to be */
					/* shifted is 33? */
	bb1	gt,   %r6,  d34		/* check to see if # of bits is 34 */
	bb1	0,    %r9,  nostky33	/* skip checking if old sticky set */
	mak	%r6,  %r11, 31<0>	/* check bits that will be shifted */
					/* into sticky */
	ff1	%r8,  %r8		/* check for ones */
	bb1	5,    %r8,  nostky33	/* do not set sticky if there are no */
					/* ones */
	set	%r9,  %r9,  1<0>	/* set new sticky bit */
nostky33:
	bb0	31,   %r11, nornd33	/* do not set round if bit is not a 1 */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd33:
	bb0	0,    %r5,  nogrd33	/* do not set guard bit if bit is not */
					/* a 1 */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
nogrd33:
	extu	%r11, %r5,  31<1>	/* shift high bits into low word */
	br.n	round			/* round and assemble result */
	 or	%r5,  %r0,  %r0		/* clear high word */

d34:
	cmp	%r6,  %r7,  34		/* is the number of bits to be */
					/* shifted 34? */
	bb1	gt,   %r6,  d35		/* check to see if # of bits is >= 35 */
	bb1	0,    %r9,  nostky34	/* skip checking if old sticky set */
	ff1	%r8,  %r11		/* check bits that will be shifted */
					/* into sticky */
	bb1	5,    %r8,  nostky34	/* do not set sticky if there are no */
					/* ones */
	set	%r9,  %r9,  1<0>	/* set new sticky bit */
nostky34:
	bb0	0,    %r5,  nornd34	/* do not set round if bit is not a 1 */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd34:
	bb0	1,    %r5,  nogrd34	/* do not set guard bit if bit is not */
					/* a 1 */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
nogrd34:
	extu	%r11, %r5,  30<2>	/* shift high bits into low word */
	br.n	round			/* round and assemble result */
	 or	%r5,  %r0,  %r0		/* clear high word */

d35:
	cmp	%r6,  %r7,  52		/* see if # of shifts is */
					/* 35 <= X <= 52 */
	bb1	gt,   %r6,  d53		/* check to see if # of shifts is 52 */
	bb1.n	0,    %r9,  nostky35	/* skip checking if old sticky set */
	 sub	%r7,  %r7,  34		/* subtract 32 from # of shifts */
					/* so that operations can be done on */
					/* the upper word, and then subtract */
					/* two more checking guard and */
					/* sticky bits */
	ff1	%r8,  %r11		/* see if lower word has a bit for */
					/* sticky */
	bb1	5,    %r8,  stkycheck35 /* see if upper word has any sticky */
					/* bits	*/
	br.n	nostky35		/* quit checking for sticky */
	 set	%r9,  %r9,  1<0>	/* set sticky bit */
stkycheck35:
	mak	%r6,  %r7,  5<5>	/* place width into width field */
	mak	%r8,  %r5,  %r6		/* mask off shifted bits - 2 */
	ff1	%r8,  %r8		/* see if r8 has any ones */
	bb1	5,    %r8,  nostky35	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky35:
	or	%r8,  %r0,  32		/* look at what does not get shifted */
					/* off plus round and sticky, */
					/* remember that the r7 value was */
					/* adjusted so that it did not */
					/* include new round or new sticky in */
					/* shifted off bits */
	subu	%r8,  %r8,  %r7		/* complement width */
	mak	%r8,  %r8,  5<5>	/* shift width into width field */
	or	%r8,  %r7,  %r8		/* add offset field */
	extu	%r11, %r5,  %r8		/* extract upper bits into low word */
	bb0	0,    %r11, nornd35	/* do not set new round bit */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd35:
	bb0	1,    %r11, nogrd35	/* do not set new guard bit */
	set	%r9,  %r9,  1<2>	/* set new guard bit */
nogrd35:
	extu	%r11, %r11, 30<2>	/* shift off remaining guard and round bits */
	br.n	round			/* round and assemble result */
	 or	%r5,  %r0,  %r0		/* clear high word */

d53:
	cmp	%r6,  %r7,  53		/* check to see if # of shifts is 53 */
	bb1	gt,   %r6,  d54		/* branch to see if shifts = 54 */
	bb1	0,    %r9,  nostky53	/* skip checking if old sticky set */
	ff1	%r8,  %r11		/* see if lower word has a bit for */
					/* sticky */
	bb1	5,    %r8,  stkycheck53 /* see if upper word has any sticky */
					/* bits */
	br.n	nostky53		/* quit checking for sticky */
	 set	%r9,  %r9,  1<0>	/* set sticky bit */
stkycheck53:
	mak	%r6,  %r5,  19<0>	/* check bits that are shifted into */
					/* sticky */
	ff1	%r8,  %r6		/* see if r6 has any ones */
	bb1	5,    %r8,  nostky53	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky53:
	bb0	19,   %r5,  nornd53	/* do not set new round bit */
	set	%r9,  %r9,  1<1>	/* set new round bit */
nornd53:
	set	%r9,  %r9,  1<2>	/* set new guard bit, this is hidden */
					/* bit */
	or	%r5,  %r0,  %r0		/* clear high word */
	br.n	round			/* round and assemble result */
	 or	%r11, %r0,  %r0		/* clear low word */

d54:
	cmp	%r6,  %r7,  54		/* check to see if # of shifts is 54 */
	bb1	gt,   %r6,  d55		/* branch to execute for shifts =>55 */
	bb1	0,    %r9,  nostky54	/* skip checking if old sticky set */
	ff1	%r8,  %r11		/* see if lower word has a bit for */
					/* sticky */
	bb1	5,    %r8,  stkycheck54 /* see if upper word has any sticky */
					/* bits */
	br.n	nostky54		/* quit checking for sticky */
	 set	%r9,  %r9,  1<0>	/* set sticky bit */
stkycheck54:
	mak	%r6,  %r5,  20<0>	/* check bits that are shifted into */
					/* sticky */
	ff1	%r8,  %r6		/* see if r6 has any ones */
	bb1	5,    %r8,  nostky54	/* do not set sticky if no ones found */
	set	%r9,  %r9,  1<0>	/* set sticky bit */
nostky54:
	set	%r9,  %r9,  1<1>	/* set new round bit, this is hidden */
					/* bit */
	clr	%r9,  %r9,  1<2>	/* clear guard bit since nothing */
					/* shifted in */
	or	%r5,  %r0,  %r0		/* clear high word */
	br.n	round			/* round and assemble result */
	 or	%r11, %r0,  %r0		/* clear low word */

d55:
	set	%r9,  %r9,  1<0>	/* set new sticky bit, this contains */
					/* hidden bit */
	clr	%r9,  %r9,  2<1>	/* clear guard and round bits since */
					/* nothing shifted in */
	or	%r5,  %r0,  %r0		/* clear high word */
	or	%r11, %r0,  %r0		/* clear low word */


/*
 * The first item that the rounding code does is see if either guard, round,
 * or sticky is set. If all are clear, then there is no denormalization loss
 * and no need to round, then branch to assemble answer.
 * For rounding, a branch table is set up. The left two most bits are the
 * rounding mode. The third bit is either the LSB of the mantissa or the
 * sign bit, depending on the rounding mode. The three LSB''s are the guard,
 * round and sticky bits.
 */

round:
	ff1	%r8,  %r9		/* see if there is denormalization */
					/* loss */
	bb1	5,    %r8,  assemble	/* no denormalization loss or */
					/* inexactness */
	extu	%r6,  %r10, 2<modelo>	/* extract rounding mode */
	bb1.n	modehi, %r10, signext	/* use sign bit instead of LSB */
	 mak	%r6,  %r6,  2<4>	/* shift over rounding mode */
	extu	%r7,  %r11, 1<0>	/* extract LSB */
	br.n	grs			/* skip sign extraction */
	 mak	%r7,  %r7,  1<3>	/* shift over LSB */
signext:
	extu	%r7,  %r10, 1<31>	/* extract sign bit */
	mak	%r7,  %r7,  1<3>	/* shift sign bit over */
grs:
	or	%r6,  %r6,  %r7
	or	%r6,  %r6,  %r9		/* or in guard, round, and sticky */
	or.u	%r1,  %r0,  %hi16(roundtable) /* form address of branch table */
	or	%r1,  %r1,  %lo16(roundtable)
	lda	%r6,  %r1[%r6]		/* scale offset into branch table */
	jmp.n	%r6			/* jump to branch table */
	 set	%r9,  %r9,  1<3>	/* set inexact flag in r9 */

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
	or	%r6,  %r0,  1		/* load a 1 into r6 so that add.co */
					/* can be used */
	add.co	%r11, %r11, %r6		/* add a one to the lower word of */
					/* result */
	bb0.n	destsize, %r12, noaddone/* single result, forget carry */
	 set	%r9,  %r9,  1<4>	/* indicate that a 1 has been added */
	add.ci	%r5,  %r5,  %r0		/* propagate carry into high word */

noaddone:
	set	%r2,  %r2,  1<inexact>
	set	%r2,  %r2,  1<underflow>

/*
 * Assemble the result of the denormalization routine for writeback to the
 * destination register. The exponent of a denormalized number is zero,
 * so simply assemble the sign and the new mantissa.
 */

assemble:
	bb1	destsize, %r12, doubassem	/* assemble double result */
	bb0	sign, %r10, exassems		/* exit assemble if sign is */
						/* zero */
	set	%r11, %r11, 1<sign>		/* make result negative */
exassems:
	br	Ureturn

doubassem:
	bb0.n	sign, %r10, signclr		/* do not set sign in r10 */
	 or	%r10, %r5,  %r0			/* load high word from r5 */
						/* into r10 */
	set	%r10, %r10, 1<sign>		/* high word with sign loaded */
signclr:
	/* FALLTHROUGH */
	/* br	Ureturn */

/* Return to fpui. */
Ureturn:
	ld	%r1,  %r31, 0			/* load return address */
	jmp	%r1

/*
 * FPoverflow
 */

ASLOCAL(FPoverflow)
	st	%r1,  %r31, 0			/* save return address */
	set	%r2,  %r2,  1<overflow>
	set	%r2,  %r2,  1<inexact>

/* Determine which rounding mode to use for the default procedure. */

	bb1	modehi, %r10, signed		/* mode is either round */
						/* toward pos. or neg. */
	bb0	modelo, %r10, OFnearest		/* rounding mode is round */
						/* nearest */
	br	OFzero				/* rounding mode is round */
						/* zero */
signed:
	bb0	modelo, %r10, OFnegative	/* rounding mode is round */
						/* negative */
	br	positive			/* rounding mode is round */
						/* positive */

/*
 * In the round toward nearest mode, positive values are rounded to
 * positive infinity and negative values are loaded toward negative infinity.
 * The value for single or double precision is loaded from a data table.
 */

OFnearest:
	bb1.n	destsize, %r12, neardouble	/* branch to neardouble of */
						/* double result */
	 mask.u	%r5,  %r10, 0x8000		/* mask off sign bit from */
						/* MANTHI */
	or.u	%r11, %r0,  %hi16(0x7f800000)	/* load single infinity */
						/* constant */
	or	%r11, %r11, %lo16(0x7f800000)
	br.n	FPof_return			/* return with result */
	 or	%r11, %r5,  %r11		/* adjust sign */
neardouble:
	or	%r11, %r0,  %r0			/* load lower word of double */
						/* infinity */
	or.u	%r10, %r0,  %hi16(0x7ff00000)	/* load upper word of double */
						/* infinity */
	or	%r10, %r10, %lo16(0x7ff00000)
	br.n	FPof_return			/* return with result */
	 or	%r10, %r5,  %r10		/* adjust sign */

/*
 * In the round toward zero mode, positive values are rounded to the largest
 * postive finite number and negative values are rounded toward the largest
 * negative finite number.
 * The value for single or double precision is loaded from a data table.
 */

OFzero:
	bb1.n	destsize, %r12, zerodouble	/* branch to zerodouble of */
						/* double result */
	 mask.u	%r5,  %r10, 0x8000		/* mask off sign bit from */
						/* MANTHI */
	or.u	%r11, %r0,  %hi16(0x7f7fffff)	/* load single finite number */
						/* constant */
	or	%r11, %r11, %lo16(0x7f7fffff)
	br.n	FPof_return			/* return with result */
	 or	%r11, %r5,  %r11		/* adjust sign */
zerodouble:
	set	%r11, %r0,  0<0>		/* load lower word of double */
						/* finite number */
	or.u	%r10, %r0,  %hi16(0x7fefffff)	/* load upper word of double */
						/* finite number */
	or	%r10, %r10, %lo16(0x7fefffff)
	br.n	FPof_return			/* return with result */
	 or	%r10, %r5,  %r10		/* adjust sign */

/*
 * In the round toward positve mode, positive values are rounded to
 * postive infinity and negative values are loaded toward the largest
 * negative finite number.
 * The value for single or double precision is loaded from a data table.
 */

positive:
	bb1	destsize, %r12, posdouble	/* branch to section for */
						/* double result */
possingle:
	bb1	sign, %r10, possingleneg	/* branch to section for */
						/* negatives */
possinglepos:
	or.u	%r11, %r0,  %hi16(0x7f800000)	/* load single infinity */
						/* constant */
	br.n	FPof_return			/* return with result */
	 or	%r11, %r11, %lo16(0x7f800000)
possingleneg:
	or.u	%r11, %r0,  %hi16(0x7f7fffff)	/* load single finite number */
						/* constant */
	or	%r11, %r11, %lo16(0x7f7fffff)
	br.n	FPof_return			/* return with result */
	 set	%r11, %r11, 1<sign>		/* set sign for negative */
posdouble:
	bb1	sign, %r10, posdoubleneg	/* branch to negative double */
						/* results */
posdoublepos:
	or	%r11, %r0,  %r0			/* load lower word of double */
						/* infinity */
	or.u	%r10, %r0,  %hi16(0x7ff00000)	/* load upper word of double */
						/* infinity */
	br.n	FPof_return			/* return with result */
	 or	%r10, %r10, %lo16(0x7ff00000)
posdoubleneg:
	set	%r11, %r0,  0<0>		/* load lower word of finite */
						/* number */
	or.u	%r10, %r0,  %hi16(0x7fefffff)	/* load upper word of finite */
						/* number */
	or	%r10, %r10, %lo16(0x7fefffff)
	br.n	FPof_return			/* return with result */
	 set	%r10, %r10, 1<sign>		/* set sign for negative */

/*
 * In the round toward negative mode, positive values are rounded to the
 * largest postive finite number and negative values are rounded to negative
 * infinity.
 * The value for single or double precision is loaded from a data table.
 */

OFnegative:
	bb1	destsize, %r12, negdouble	/* branch to section for */
						/* double result */
negsingle:
	bb1	sign, %r10, negsingleneg	/* branch to section for */
						/* negatives */
negsinglepos:
	or.u	%r11, %r0,  %hi16(0x7f7fffff)	/* load single finite number */
						/* constant */
	br.n	FPof_return			/* return with result */
	 or	%r11, %r11, %lo16(0x7f7fffff)
negsingleneg:
	or.u	%r11, %r0,  %hi16(0x7f800000)	/* load single infinity */
						/* constant */
	or	%r11, %r11, %lo16(0x7f800000)
	br.n	FPof_return			/* return with result */
	 set	%r11, %r11, 1<sign>		/* set sign for negative */
negdouble:
	bb1	sign, %r10, negdoubleneg	/* branch to negative double */
						/* results */
negdoublepos:
	set	%r11, %r0,  0<0>		/* load lower word of finite */
						/* number */
	or.u	%r10, %r0,  %hi16(0x7fefffff)	/* load upper word of finite */
						/* number */
	br.n	FPof_return			/* return with result */
	 or	%r10, %r10, %lo16(0x7fefffff)
negdoubleneg:
	or	%r11, %r0,  %r0			/* load lower word of double */
						/* infinity */
	or.u	%r10, %r0,  %hi16(0x7ff00000)	/* load upper word of double */
						/* infinity */
	or	%r10, %r10, %lo16(0x7ff00000)
	set	%r10, %r10, 1<sign>		/* set sign for negative */

FPof_return:
	ld	%r1,  %r31, 0			/* load return address */
	jmp	%r1

ENTRY(m88100_fpu_imprecise_exception)
	or	%r29, %r2,  %r0			/* r29 is now the E.F. */
	subu	%r31, %r31, 16
	st	%r1,  %r31, 4
	st	%r29, %r31, 8

	ld	%r2 , %r29, EF_FPSR
	ld	%r3 , %r29, EF_FPCR
	ld	%r4 , %r29, EF_FPECR
	ld	%r10, %r29, EF_FPRH
	ld	%r11, %r29, EF_FPRL
	ld	%r12, %r29, EF_FPIT

/*
 * Load into r1 the return address for the exception handlers. Looking
 * at FPECR, branch to the appropriate exception handler.
 */

	or.u	%r1,  %r0,  %hi16(fpui_wrapup)	/* load return address of */
	or	%r1,  %r1,  %lo16(fpui_wrapup)	/* functions */

	bb0	2,    %r4,  2f			/* branch to FPunderflow if */
	br	_ASM_LABEL(FPunderflow)		/* bit set */
2:
	bb0	1,    %r4,  3f			/* branch to FPoverflow if */
	br	_ASM_LABEL(FPoverflow)		/* bit set */
3:
	/* XXX handle inexact!!! */

fpui_wrapup:
	FLUSH_PIPELINE				/* make sure all floating */
						/* point operations have */
						/* finished */
	ldcr	%r4,  %cr1			/* load the PSR */
#if 0
	set	%r4,  %r4,  1<PSR_FPU_DISABLE_BIT>
#endif
	set	%r4,  %r4,  1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	%r4,  %cr1
	ld	%r1,  %r31, 4
	ld	%r29, %r31, 8
	addu	%r31, %r31, 16

	fstcr	%r2, FPSR		/* write revised value of FPSR... */
	fstcr	%r3, FPCR		/* ...and FPCR... */
	st	%r2, %r29, EF_FPSR	/* ...into the trapframe as well */
	st	%r3, %r29, EF_FPCR

	/* write back the results */
	extu	%r2,  %r12, 5<0>
	bb0.n	destsize, %r12, Iwritesingle
	 addu	%r3,  %r29, EF_R0
	st	%r10, %r3[%r2]
	addu	%r2,  %r2,  1
	clr	%r2,  %r2,  27<5>
Iwritesingle:
	jmp.n	%r1
	 st	%r11, %r3[%r2]
