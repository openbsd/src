/*	$OpenBSD: remU.S,v 1.1.1.1 1998/06/23 18:56:54 mickey Exp $	*/

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
 * Author: Committee
 *
 * 
 * ROUTINE:	$$remU
 * 
 * 	Single precision divide for remainder with unsigned binary integers.
 * 
 * 	The remainder must be dividend-(dividend/divisor)*divisor.
 * 	Divide by zero is trapped.
 * 
 * INPUT REGISTERS:
 * 	arg0 ==	dividend
 * 	arg1 == divisor
 * 	r31  == return pc
 * 	sr0  == return space when called externally
 * 
 * OUTPUT REGISTERS:
 * 	arg0 =  undefined
 * 	arg1 =  undefined
 * 	ret1 =  remainder
 * 
 * OTHER REGISTERS AFFECTED:
 * 	r1   =  undefined
 * 
 * SIDE EFFECTS:
 * 	Causes a trap under the following conditions:  DIVIDE BY ZERO
 * 	Changes memory at the following places:  NONE
 * 
 * PERMISSIBLE CONTEXT:
 * 	Unwindable.
 * 	Does not create a stack frame.
 * 	Suitable for internal or external millicode.
 * 	Assumes the special millicode register conventions.
 * 
 * DISCUSSION:
 * 	Calls other millicode routines using r31: NONE
 * 	Calls other millicode routines: NONE
 */

DEFINE(temp,r1)
DEFINE(rmndr,ret1)	; r29
	.subspa	$MILLICODE$
	.export $$remU,millicode
	.proc
	.callinfo	millicode

	.entry
$$remU

	comib,>=,n  0,arg1,special_case
	sub	r0,arg1,rmndr		; clear carry, negate the divisor
	ds	r0,rmndr,r0		; set V-bit to 1
	add	arg0,arg0,temp		; shift msb bit into carry
	ds	r0,arg1,rmndr		; 1st divide step, if no carry
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 2nd divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 3rd divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 4th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 5th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 6th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 7th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 8th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 9th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 10th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 11th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 12th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 13th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 14th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 15th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 16th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 17th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 18th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 19th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 20th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 21st divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 22nd divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 23rd divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 24th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 25th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 26th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 27th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 28th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 29th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 30th divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 31st divide step
	addc	temp,temp,temp		; shift temp with/into carry
	ds	rmndr,arg1,rmndr		; 32nd divide step,
	comiclr,<= 0,rmndr,r0
	  add	rmndr,arg1,rmndr	; correction
	MILLIRETN
	nop
; Putting >= on the last DS and deleting COMICLR does not work!


special_case
	addit,=	0,arg1,r0		; trap on div by zero
	sub,>>=	arg0,arg1,rmndr
	  copy	arg0,rmndr
	.exit
	MILLIRETN
	nop
	.procend
	.end
