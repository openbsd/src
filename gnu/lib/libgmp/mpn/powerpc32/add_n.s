# PowerPC-32 __mpn_add_n -- Add two limb vectors of equal, non-zero length.

# Copyright (C) 1992, 1994, 1995 Free Software Foundation, Inc.

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


# INPUT PARAMETERS
# res_ptr	r3
# s1_ptr	r4
# s2_ptr	r5
# size		r6

	.toc
	.extern __mpn_add_n[DS]
	.extern .__mpn_add_n
.csect [PR]
	.align 2
	.globl __mpn_add_n
	.globl .__mpn_add_n
	.csect __mpn_add_n[DS]
__mpn_add_n:
	.long .__mpn_add_n, TOC[tc0], 0
	.csect [PR]
.__mpn_add_n:
	mtctr	6		# copy size into CTR
	lwz	8,0(4)		# load least significant s1 limb
	lwz	0,0(5)		# load least significant s2 limb
	addi	3,3,-4		# offset res_ptr, it's updated before used
	addc	7,0,8		# add least significant limbs, set cy
	bdz	Lend		# If done, skip loop
Loop:	lwzu	8,4(4)		# load s1 limb and update s1_ptr
	lwzu	0,4(5)		# load s2 limb and update s2_ptr
	stwu	7,4(3)		# store previous limb in load latency slot
	adde	7,0,8		# add new limbs with cy, set cy
	bdnz	Loop		# decrement CTR and loop back
Lend:	stw	7,4(3)		# store ultimate result limb
	li	3,0		# load cy into ...
	addze	3,3		# ... return value register
	blr
