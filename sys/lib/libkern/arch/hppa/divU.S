/*	$OpenBSD: divU.S,v 1.1.1.1 1998/06/23 18:56:53 mickey Exp $	*/

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
 * ROUTINE:	$$divU
 * 
 * 	Single precision divide for unsigned integers.
 * 
 * 	Quotient is truncated towards zero.
 * 	Traps on divide by zero.
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
 * 		divisor is zero
 * 	Changes memory at the following places:
 * 		NONE
 * 
 * PERMISSIBLE CONTEXT:
 * 	Unwindable.
 * 	Does not create a stack frame.
 * 	Suitable for internal or external millicode.
 * 	Assumes the special millicode register conventions.
 * 
 * DISCUSSION:
 * 	Branchs to other millicode routines using BE:
 * 		$$divU_# for 3,5,6,7,9,10,12,14,15
 * 
 * 	For selected small divisors calls the special divide by constant
 * 	routines written by Karl Pettis.  These are: 3,5,6,7,9,10,12,14,15.
 * 
 */

DEFINE(temp,r1)
DEFINE(retreg,ret1)	; r29
	.subspa	$MILLICODE$
	.export $$divU,millicode
	.import $$divU_3,millicode
	.import $$divU_5,millicode
	.import $$divU_6,millicode
	.import $$divU_7,millicode
	.import $$divU_9,millicode
	.import $$divU_10,millicode
	.import $$divU_12,millicode
	.import $$divU_14,millicode
	.import $$divU_15,millicode
	.proc
	.callinfo	millicode
$$divU
; The subtract is not nullified since it does no harm and can be used
; by the two cases that branch back to "normal".
	comib,>=  15,arg1,special_divisor
	sub	r0,arg1,temp		; clear carry, negate the divisor
	ds	r0,temp,r0		; set V-bit to 1
normal
	add	arg0,arg0,retreg	; shift msb bit into carry
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
	MILLIRET
	addc	retreg,retreg,retreg	; shift last retreg bit into retreg
;_____________________________________________________________________________
; handle the cases where divisor is a small constant or has high bit on
special_divisor
	blr	arg1,r0
	comib,>,n  0,arg1,big_divisor	; nullify previous instruction
zero_divisor	; this label is here to provide external visibility
	addit,=	0,arg1,0		; trap for zero dvr
	nop
	MILLIRET			; divisor == 1
	copy	arg0,retreg
	MILLIRET			; divisor == 2
	extru	arg0,30,31,retreg
	MILLI_BEN($$divU_3)		; divisor == 3
	nop
	MILLIRET			; divisor == 4
	extru	arg0,29,30,retreg
	MILLI_BEN($$divU_5)		; divisor == 5
	nop
	MILLI_BEN($$divU_6)		; divisor == 6
	nop
	MILLI_BEN($$divU_7)		; divisor == 7
	nop
	MILLIRET			; divisor == 8
	extru	arg0,28,29,retreg
	MILLI_BEN($$divU_9)		; divisor == 9
	nop
	MILLI_BEN($$divU_10)		; divisor == 10
	nop
	b	normal			; divisor == 11
	ds	r0,temp,r0		; set V-bit to 1
	MILLI_BEN($$divU_12)		; divisor == 12
	nop
	b	normal			; divisor == 13
	ds	r0,temp,r0		; set V-bit to 1
	MILLI_BEN($$divU_14)		; divisor == 14
	nop
	MILLI_BEN($$divU_15)		; divisor == 15
	nop
;_____________________________________________________________________________
; Handle the case where the high bit is on in the divisor.
; Compute:	if( dividend>=divisor) quotient=1; else quotient=0;
; Note:		dividend>==divisor iff dividend-divisor does not borrow
; and		not borrow iff carry
big_divisor
	sub	arg0,arg1,r0
	MILLIRET
	addc	r0,r0,retreg
	.procend
	.end
