# VAX __mpn_add_n -- Add two limb vectors of the same length > 0 and store
# sum in a third limb vector.

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


# INPUT PARAMETERS
# res_ptr	(sp + 4)
# s1_ptr	(sp + 8)
# s2_ptr	(sp + 12)
# size		(sp + 16)

.text
	.align 1
.globl ___mpn_add_n
___mpn_add_n:
	.word	0x0
	movl	16(ap),r0
	movl	12(ap),r1
	movl	8(ap),r2
	movl	4(ap),r3
	subl2	r4,r4

Loop:
	movl	(r2)+,r4
	adwc	(r1)+,r4
	movl	r4,(r3)+
	jsobgtr	r0,Loop

	adwc	r0,r0
	ret
