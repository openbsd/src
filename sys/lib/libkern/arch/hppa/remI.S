/*	$OpenBSD: remI.S,v 1.1.1.1 1998/06/23 18:56:54 mickey Exp $	*/

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
 * Editor: Michael J. Eager
 * 
 * 
 * ROUTINE:	$$remI
 *  
 * DESCRIPTION:	
 * 
 * 	$$remI returns the remainder of the division of two signed 32-bit
 * 	integers.  The sign of the remainder is the same as the sign of
 * 	the dividend.
 * 
 * 
 * INPUT REGISTERS:
 * 
 * 	arg0 == dividend
 * 	arg1 == divisor
 * 	r31  == return pc
 * 	sr0  == return space when called externally 
 * 
 * OUTPUT REGISTERS:
 * 
 * 	arg0 = destroyed
 * 	arg1 = destroyed
 * 	ret1 = remainder
 * 
 * OTHER REGISTERS AFFECTED:
 * 
 * 	r1   = undefined
 * 
 * SIDE EFFECTS:
 * 
 * 	Causes a trap under the following conditions:  DIVIDE BY ZERO
 * 	Changes memory at the following places:  NONE
 * 
 * PERMISSIBLE CONTEXT:
 * 
 * 	Unwindable
 * 	Does not create a stack frame
 * 	Is usable for internal or external microcode
 * 
 * DISCUSSION:
 * 
 * 	Calls other millicode routines via r31:  NONE
 * 	Calls other millicode routines:  NONE
 */

DEFINE(tmp,r1)
DEFINE(retreg,ret1)

	.subspa	$MILLICODE$
	.proc
	.callinfo millicode
	.entry
$$remI
	.export $$remI,MILLICODE
	addit,=		0,arg1,0		; trap if div by zero
	add,>=		0,arg0,retreg		; move dividend, if retreg < 0,
	sub		0,retreg,retreg		;   make it positive
	sub		0,arg1, tmp		; clear carry, 
						;   negate the divisor
	ds		0, tmp,0		; set V-bit to the comple-
						;   ment of the divisor sign
	or		0,0, tmp		; clear  tmp
	add		retreg,retreg,retreg	; shift msb bit into carry
	ds		 tmp,arg1, tmp		; 1st divide step, if no carry
						;   out, msb of quotient = 0
	addc		retreg,retreg,retreg	; shift retreg with/into carry
tt1	ds		 tmp,arg1, tmp		; 2nd divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 3rd divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 4th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 5th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 6th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 7th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 8th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 9th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 10th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 11th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 12th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 13th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 14th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 15th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 16th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 17th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 18th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 19th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 20th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 21st divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 22nd divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 23rd divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 24th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 25th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 26th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 27th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 28th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 29th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 30th divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 31st divide step
	addc		retreg,retreg,retreg	; shift retreg with/into carry
	ds		 tmp,arg1, tmp		; 32nd divide step,
	addc		retreg,retreg,retreg	; shift last bit into retreg
	movb,>=,n	 tmp,retreg,finish	; branch if pos.  tmp
	add,<		arg1,0,0		; if arg1 > 0, add arg1
	add,tr		 tmp,arg1,retreg	;   for correcting remainder tmp
	sub		 tmp,arg1,retreg	; else add absolute value arg1
finish	add,>=		arg0,0,0		; set sign of remainder
	sub		0,retreg,retreg		;   to sign of dividend
	.exit
	MILLIRET
	nop
	.procend
#ifdef milliext
	.origin 0x00000200
#endif
	.end
