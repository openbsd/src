! SH2 __mpn_mul_1 -- Multiply a limb vector with a limb and store
! the result in a second limb vector.

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
! size		r6
! s2_limb	r7

	.text
	.align 1
	.global	___mpn_mul_1
___mpn_mul_1:
	mov	#0,r2		! cy_limb = 0
	mov	#0,r0		! Keep r0 = 0 for entire loop
	clrt

Loop:	mov.l	@r5+,r3
	dmulu.l	r3,r7
	sts	macl,r1
	addc	r2,r1
	sts	mach,r2
	addc	r0,r2		! propagate carry to cy_limb (dt clobbers T)
	dt	r6
	mov.l	r1,@r4
	bf.s	Loop
	add	#4,r4

	rts
	mov	r2,r0
