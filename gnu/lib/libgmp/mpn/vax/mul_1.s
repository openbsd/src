# VAX __mpn_mul_1 -- Multiply a limb vector with a limb and store
# the result in a second limb vector.

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
# size		(sp + 12)
# s2_limb	(sp + 16)

.text
	.align 1
.globl ___mpn_mul_1
___mpn_mul_1:
	.word	0xfc0
	movl	12(ap),r4
	movl	8(ap),r8
	movl	4(ap),r9
	movl	16(ap),r6
	jlss	s2_big

# One might want to combine the addl2 and the store below, but that
# is actually just slower according to my timing tests.  (VAX 3600)

	clrl	r3
	incl	r4
	ashl	$-1,r4,r7
	jlbc	r4,L1
	clrl	r11

# Loop for S2_LIMB < 0x80000000
Loop1:	movl	(r8)+,r1
	jlss	L1n0
	emul	r1,r6,$0,r2
	addl2	r11,r2
	adwc	$0,r3
	movl	r2,(r9)+
L1:	movl	(r8)+,r1
	jlss	L1n1
L1p1:	emul	r1,r6,$0,r10
	addl2	r3,r10
	adwc	$0,r11
	movl	r10,(r9)+

	jsobgtr	r7,Loop1
	movl	r11,r0
	ret

L1n0:	emul	r1,r6,$0,r2
	addl2	r11,r2
	adwc	r6,r3
	movl	r2,(r9)+
	movl	(r8)+,r1
	jgeq	L1p1
L1n1:	emul	r1,r6,$0,r10
	addl2	r3,r10
	adwc	r6,r11
	movl	r10,(r9)+

	jsobgtr	r7,Loop1
	movl	r11,r0
	ret


s2_big:	clrl	r3
	incl	r4
	ashl	$-1,r4,r7
	jlbc	r4,L2
	clrl	r11

# Loop for S2_LIMB >= 0x80000000
Loop2:	movl	(r8)+,r1
	jlss	L2n0
	emul	r1,r6,$0,r2
	addl2	r11,r2
	adwc	r1,r3
	movl	r2,(r9)+
L2:	movl	(r8)+,r1
	jlss	L2n1
L2p1:	emul	r1,r6,$0,r10
	addl2	r3,r10
	adwc	r1,r11
	movl	r10,(r9)+

	jsobgtr	r7,Loop2
	movl	r11,r0
	ret

L2n0:	emul	r1,r6,$0,r2
	addl2	r1,r3
	addl2	r11,r2
	adwc	r6,r3
	movl	r2,(r9)+
	movl	(r8)+,r1
	jgeq	L2p1
L2n1:	emul	r1,r6,$0,r10
	addl2	r1,r11
	addl2	r3,r10
	adwc	r6,r11
	movl	r10,(r9)+

	jsobgtr	r7,Loop2
	movl	r11,r0
	ret
