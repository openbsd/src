# ns32000 __mpn_sub_n -- Subtract two limb vectors of the same length > 0 and
# store difference in a third limb vector.

# Copyright (C) 1992, 1994 Free Software Foundation, Inc.

# This file is part of the GNU MP Library.

# The GNU MP Library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Library General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.

# The GNU MP Library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
# License for more details.

# You should have received a copy of the GNU Library General Public License
# along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA.


	.align 1
.globl ___mpn_sub_n
___mpn_sub_n:
	save	[r3,r4,r5]
	negd	28(sp),r3
	movd	r3,r0
	lshd	2,r0
	movd	24(sp),r4
	subd	r0,r4			# r4 -> to end of S2
	movd	20(sp),r5
	subd	r0,r5			# r5 -> to end of S1
	movd	16(sp),r2
	subd	r0,r2			# r2 -> to end of RES
	subd	r0,r0			# cy = 0

Loop:	movd	r5[r3:d],r0
	subcd	r4[r3:d],r0
	movd	r0,r2[r3:d]
	acbd	1,r3,Loop

	scsd	r0			# r0 = cy.
	restore	[r5,r4,r3]
	ret	0
