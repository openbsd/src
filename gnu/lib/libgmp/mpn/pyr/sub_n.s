# Pyramid __mpn_sub_n -- Subtract two limb vectors of the same length > 0 and
# store difference in a third limb vector.

# Copyright (C) 1995 Free Software Foundation, Inc.

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

.text
	.align	2
.globl	___mpn_sub_n
___mpn_sub_n:
	movw	$-1,tr0		# representation for carry clear

	movw	pr3,tr2
	andw	$3,tr2
	beq	Lend0
	subw	tr2,pr3

Loop0:	rsubw	$0,tr0		# restore carry bit from carry-save register

	movw	(pr1),tr1
	subwb	(pr2),tr1
	movw	tr1,(pr0)

	subwb	tr0,tr0
	addw	$4,pr0
	addw	$4,pr1
	addw	$4,pr2
	addw	$-1,tr2
	bne	Loop0

	mtstw	pr3,pr3
	beq	Lend
Lend0:
Loop:	rsubw	$0,tr0		# restore carry bit from carry-save register

	movw	(pr1),tr1
	subwb	(pr2),tr1
	movw	tr1,(pr0)

	movw	4(pr1),tr1
	subwb	4(pr2),tr1
	movw	tr1,4(pr0)

	movw	8(pr1),tr1
	subwb	8(pr2),tr1
	movw	tr1,8(pr0)

	movw	12(pr1),tr1
	subwb	12(pr2),tr1
	movw	tr1,12(pr0)

	subwb	tr0,tr0
	addw	$16,pr0
	addw	$16,pr1
	addw	$16,pr2
	addw	$-4,pr3
	bne	Loop
Lend:
	mnegw	tr0,pr0
	ret
