! SH __mpn_add_n -- Add two limb vectors of the same length > 0 and store
! sum in a third limb vector.

! Copyright (C) 1995 Free Software Foundation, Inc.

! This file is part of the GNU MP Library.

! The GNU MP Library is free software; you can redistribute it and/or modify
! it under the terms of the GNU Library General Public License as published by
! the Free Software Foundation; either version 2 of the License, or (at your
! option) any later version.

! The GNU MP Library is distributed in the hope that it will be useful, but
! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
! or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
! License for more details.

! You should have received a copy of the GNU Library General Public License
! along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
! the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
! MA 02111-1307, USA.


! INPUT PARAMETERS
! res_ptr	r4
! s1_ptr	r5
! s2_ptr	r6
! size		r7

	.text
	.align 2
	.global	___mpn_add_n
___mpn_add_n:
	mov	#0,r3		! clear cy save reg

Loop:	mov.l	@r5+,r1
	mov.l	@r6+,r2
	shlr	r3		! restore cy
	addc	r2,r1
	movt	r3		! save cy
	mov.l	r1,@r4
	dt	r7
	bf.s	Loop
	 add	#4,r4

	rts
	movt	r0		! return carry-out from most sign. limb
