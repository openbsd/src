! Z8000 (32 bit limb version) __mpn_sub_n -- Subtract two limb vectors of the
! same length > 0 and store difference in a third limb vector.

! Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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
! res_ptr	r7
! s1_ptr	r6
! s2_ptr	r5
! size		r4

! If we are really crazy, we can use push to write a few result words
! backwards, using push just because it is faster than reg+disp.  We'd
! then add 2x the number of words written to r7...

	segm
	.text
	even
	global ___mpn_sub_n
___mpn_sub_n:
	popl	rr0,@r6
	popl	rr8,@r5
	subl	rr0,rr8
	ldl	@r7,rr0
	dec	r4
	jr	eq,Lend
Loop:	popl	rr0,@r6
	popl	rr8,@r5
	sbc	r1,r9
	sbc	r0,r8
	inc	r7,#4
	ldl	@r7,rr0
	dec	r4
	jr	ne,Loop
Lend:	ld	r2,r4		! use 0 already in r4
	ld	r3,r4
	adc	r2,r2
	ret	t
