/*	$OpenBSD: divI.S,v 1.1.1.1 1998/06/23 18:56:53 mickey Exp $	*/

/*
 *  (c) Copyright 1985 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

#include "prefix.h"

/*
 *
 * ROUTINES:	$$divI, $$divoI
 * 
 * Single precision divide for singed binary integers.
 * 
 * The quotient is truncated towards zero.
 * The sign of the quotient is the XOR of the signs of the dividend and
 * divisor.
 * Divide by zero is trapped.
 * Divide of -2**31 by -1 is trapped for $$divoI but not for $$divI.
 * 
 * INPUT REGISTERS:
 * 	arg0 ==	dividend
 * 	arg1 ==	divisor
 * 	r31  == return pc
 * 	sr0  == return space when called externally
 * 
 * OUTPUT REGISTERS:
 * 	arg0 =  undefined
 * 	arg1 =  undefined
 * 	ret1 =  quotient
 * 
 * OTHER REGISTERS AFFECTED:
 * 	r1   =  undefined
 * 
 * SIDE EFFECTS:
 * 	Causes a trap under the following conditions:
 * 		divisor is zero  (traps with ADDIT,=  0,25,0)
 * 		dividend==-2**31  and divisor==-1 and routine is $$divoI
 * 				 (traps with ADDO  26,25,0)
 * 	Changes memory at the following places:
 * 		NONE
 * 
 * PERMISSIBLE CONTEXT:
 * 	Unwindable.
 * 	Suitable for internal or external millicode.
 * 	Assumes the special millicode register conventions.
 * 
 * DISCUSSION:
 * 	Branchs to other millicode routines using BE
 * 		$$div_# for # being 2,3,4,5,6,7,8,9,10,12,14,15
 * 
 * 	For selected divisors, calls a divide by constant routine written by
 * 	Karl Pettis.  Eligible divisors are 1..15 excluding 11 and 13.
 * 
 * 	The only overflow case is -2**31 divided by -1.
 * 	Both routines return -2**31 but only $$divoI traps.
 */

DEFINE(temp,r1)
DEFINE(retreg,ret1)	; r29
	.import $$divI_2,millicode
	.import $$divI_3,millicode
	.import $$divI_4,millicode
	.import $$divI_5,millicode
	.import $$divI_6,millicode
	.import $$divI_7,millicode
	.import $$divI_8,millicode
	.import $$divI_9,millicode
	.import $$divI_10,millicode
	.import $$divI_12,millicode
	.import $$divI_14,millicode
	.import $$divI_15,millicode
	.export $$divI,millicode
	.export	$$divoI,millicode
	.proc
	.callinfo	millicode
$$divoI
	comib,=,n  -1,arg1,negative1	; when divisor == -1
$$divI
	comib,>>=,n 15,arg1,small_divisor
	add,>=	0,arg0,retreg		; move dividend, if retreg < 0,
normal
	  sub	0,retreg,retreg		;   make it positive
	sub	0,arg1,temp		; clear carry, 
					;   negate the divisor
	ds	0,temp,0		; set V-bit to the comple-
					;   ment of the divisor sign
	add	retreg,retreg,retreg	; shift msb bit into carry
	ds	r0,arg1,temp		; 1st divide step, if no carry
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 2nd divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 3rd divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 4th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 5th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 6th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 7th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 8th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 9th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 10th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 11th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 12th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 13th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 14th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 15th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 16th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 17th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 18th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 19th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 20th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 21st divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 22nd divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 23rd divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 24th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 25th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 26th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 27th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 28th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 29th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 30th divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 31st divide step
	addc	retreg,retreg,retreg	; shift retreg with/into carry
	ds	temp,arg1,temp		; 32nd divide step,
	addc	retreg,retreg,retreg	; shift last retreg bit into retreg
	xor,>=	arg0,arg1,0		; get correct sign of quotient
	  sub	0,retreg,retreg		;   based on operand signs
	MILLIRETN
	nop
;______________________________________________________________________
small_divisor
	blr,n	arg1,r0
	nop
; table for divisor == 0,1, ... ,15
	addit,=	0,arg1,r0	; trap if divisor == 0
	nop
	MILLIRET		; divisor == 1
	copy	arg0,retreg
	MILLI_BEN($$divI_2)	; divisor == 2
	nop
	MILLI_BEN($$divI_3)	; divisor == 3
	nop
	MILLI_BEN($$divI_4)	; divisor == 4
	nop
	MILLI_BEN($$divI_5)	; divisor == 5
	nop
	MILLI_BEN($$divI_6)	; divisor == 6
	nop
	MILLI_BEN($$divI_7)	; divisor == 7
	nop
	MILLI_BEN($$divI_8)	; divisor == 8
	nop
	MILLI_BEN($$divI_9)	; divisor == 9
	nop
	MILLI_BEN($$divI_10)	; divisor == 10
	nop
	b	normal		; divisor == 11
	add,>=	0,arg0,retreg
	MILLI_BEN($$divI_12)	; divisor == 12
	nop
	b	normal		; divisor == 13
	add,>=	0,arg0,retreg
	MILLI_BEN($$divI_14)	; divisor == 14
	nop
	MILLI_BEN($$divI_15)	; divisor == 15
	nop
;______________________________________________________________________
negative1
	sub	0,arg0,retreg	; result is negation of dividend
	MILLIRET
	addo	arg0,arg1,r0	; trap iff dividend==0x80000000 && divisor==-1
	.procend
	.end
