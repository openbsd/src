/*	$OpenBSD: mulU.S,v 1.1.1.1 1998/06/23 18:56:54 mickey Exp $	*/

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
 * ROUTINE:	$$mulU
 * 
 * 
 * DESCRIPTION:	
 * 
 * 	$$mulU multiplies two single word unsigned integers, giving a single 
 * 	word result.  
 * 
 * 
 * INPUT REGISTERS:
 * 
 * 	arg0 = Operand 1
 * 	arg1 = Operand 2
 * 	r31  == return pc
 * 	sr0  == return space when called externally 
 * 
 * 
 * OUTPUT REGISTERS:
 * 
 * 	arg0 = undefined
 * 	arg1 = undefined
 * 	ret1 = result 
 * 
 * OTHER REGISTERS AFFECTED:
 * 
 * 	r1   = undefined
 * 
 * SIDE EFFECTS:
 * 
 * 	Causes a trap under the following conditions:  NONE
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
$$mulU
	.export $$mulU,MILLICODE

	combt,<<=	arg1,arg0,$loop
	add		0,0,retreg
	add		0,arg1,tmp
	add		0,arg0,arg1
	add		0,tmp,arg0
$loop	extru		arg1,31,4,tmp	
	blr		tmp,0		
	extru		arg1,27,28,arg1	
	comb,=,n	arg1,0,$return	
	b,n		$shmcand		
	movb,tr		arg0,tmp,$addtmp
	comb,=		arg1,0,$return	
$two	b		$endloop		
	sh1add		arg0,0,tmp	
	b		$endloop		
	sh1add		arg0,arg0,tmp	
	b		$endloop		
	sh2add		arg0,0,tmp	
	b		$endloop		
	sh2add		arg0,arg0,tmp	
	b		$fiftnx		
	sh1add		arg0,0,tmp	
	movb,tr		arg0,tmp,$fiftnx
	sh2add		arg0,retreg,retreg	
	b		$endloop		
	sh3add		arg0,0,tmp	
	b		$endloop		
	sh3add		arg0,arg0,tmp	
	b		$two		
	sh3add		arg0,retreg,retreg	
	movb,tr		arg0,tmp,$fiftnx
	sh3add		arg0,retreg,retreg	
	b		$fiftnx		
	sh2add		arg0,0,tmp	
	sh2add		arg0,arg0,tmp	
	b		$endloop		
	sh3add		arg0,retreg,retreg	
	sh1add,tr	arg0,0,tmp	
	sh2add		arg0,arg0,tmp	
$fiftnx	sh1add		tmp,tmp,tmp	
$endloop comb,=		arg1,0,$return	
$addtmp	add		tmp,retreg,retreg	
$shmcand addb,tr	arg0,arg0,$loop
	sh3add		arg0,0,arg0	
	.exit
$return	MILLIRET
	nop
	.procend
#ifdef milliext
	.origin 0x00000100
#endif
	.end
