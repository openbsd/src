# Pyramid __mpn_mul_1 -- Multiply a limb vector with a limb and store
# the result in a second limb vector.

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
.globl	___mpn_mul_1
___mpn_mul_1:
	mova	(pr0)[pr2*4],pr0
	mova	(pr1)[pr2*4],pr1
	mnegw	pr2,pr2
	movw	$0,tr3

Loop:	movw	(pr1)[pr2*4],tr1
	uemul	pr3,tr0
	addw	tr3,tr1
	movw	$0,tr3
	addwc	tr0,tr3
	movw	tr1,(pr0)[pr2*4]
	addw	$1,pr2
	bne	Loop

	movw	tr3,pr0
	ret
