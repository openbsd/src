/* mc68020 __mpn_add_n -- Add two limb vectors of the same length > 0 and store
   sum in a third limb vector.

Copyright (C) 1992, 1994, 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

/*
  INPUT PARAMETERS
  res_ptr	(sp + 4)
  s1_ptr	(sp + 8)
  s2_ptr	(sp + 16)
  size		(sp + 12)
*/

#include "sysdep.h"
#include "asm-syntax.h"

	TEXT
	ALIGN
	GLOBL	C_SYMBOL_NAME(__mpn_add_n)

C_SYMBOL_NAME(__mpn_add_n:)
PROLOG(__mpn_add_n)
/* Save used registers on the stack.  */
	movel	R(d2),MEM_PREDEC(sp)
	movel	R(a2),MEM_PREDEC(sp)

/* Copy the arguments to registers.  Better use movem?  */
	movel	MEM_DISP(sp,12),R(a2)
	movel	MEM_DISP(sp,16),R(a0)
	movel	MEM_DISP(sp,20),R(a1)
	movel	MEM_DISP(sp,24),R(d2)

	eorw	#1,R(d2)
	lsrl	#1,R(d2)
	bcc	L(L1)
	subql	#1,R(d2)	/* clears cy as side effect */

L(Loop:)
	movel	MEM_POSTINC(a0),R(d0)
	movel	MEM_POSTINC(a1),R(d1)
	addxl	R(d1),R(d0)
	movel	R(d0),MEM_POSTINC(a2)
L(L1:)	movel	MEM_POSTINC(a0),R(d0)
	movel	MEM_POSTINC(a1),R(d1)
	addxl	R(d1),R(d0)
	movel	R(d0),MEM_POSTINC(a2)

	dbf	R(d2),L(Loop)		/* loop until 16 lsb of %4 == -1 */
	subxl	R(d0),R(d0)	/* d0 <= -cy; save cy as 0 or -1 in d0 */
	subl	#0x10000,R(d2)
	bcs	L(L2)
	addl	R(d0),R(d0)	/* restore cy */
	bra	L(Loop)

L(L2:)
	negl	R(d0)

/* Restore used registers from stack frame.  */
	movel	MEM_POSTINC(sp),R(a2)
	movel	MEM_POSTINC(sp),R(d2)

	rts
EPILOG(__mpn_add_n)
