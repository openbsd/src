#	$OpenBSD: bn_asm_vax.S,v 1.1 2003/11/18 12:39:05 markus Exp $
#	$NetBSD: bn_asm_vax.S,v 1.1 2003/11/03 10:22:28 ragge Exp $

#include <machine/asm.h>

# w.j.m. 15-jan-1999
#
# it's magic ...
#
# ULONG bn_mul_add_words(ULONG r[],ULONG a[],int n,ULONG w) {
#	ULONG c = 0;
#	int i;
#	for(i = 0; i < n; i++) <c,r[i]> := r[i] + c + a[i] * w ;
#	return c;
# }

ENTRY(bn_mul_add_words,R6)
	movl	4(ap),r2		# *r
	movl	8(ap),r3		# *a
	movl	12(ap),r4		# n
	movl	16(ap),r5		# w
	clrl	r6			# return value ("carry")

0:	emul	r5,(r3),(r2),r0	# w * a[0] + r[0] -> r0

	# fixup for "negative" r[]
	tstl	(r2)
	bgeq	1f
	incl	r1			# add 1 to highword

1:	# add saved carry to result
	addl2	r6,r0
	adwc	$0,r1

	# combined fixup for "negative" w, a[]
	tstl	r5		# if w is negative...
	bgeq	1f
	addl2	(r3),r1		# ...add a[0] again to highword
1:	tstl	(r3)		# if a[0] is negative...
	bgeq	1f
	addl2	r5,r1		# ...add w again to highword
1:
	movl	r0,(r2)+	# save low word in dest & advance *r
	addl2	$4,r3		# advance *a
	movl	r1,r6		# high word in r6 for return value

	sobgtr	r4,0b		# loop?

	movl	r6,r0
	ret

#	.title	vax_bn_mul_words  unsigned multiply & add, 32*32+32=>64
#;
#; w.j.m. 15-jan-1999
#;
#; it's magic ...
#;
#; ULONG bn_mul_words(ULONG r[],ULONG a[],int n,ULONG w) {
#;	ULONG c = 0;
#;	int i;
#;	for(i = 0; i < num; i++) <c,r[i]> := a[i] * w + c ;
#;	return(c);
#; }
#

ENTRY(bn_mul_words,R6)
	movl	4(ap),r2		# *r
	movl	8(ap),r3		# *a
	movl	12(ap),r4		# n
	movl	16(ap),r5		# w
	clrl	r6			# carry

0:	emul	r5,(r3),r6,r0		# w * a[0] + carry -> r0

	# fixup for "negative" carry
	tstl	r6
	bgeq	1f
	incl	r1

1:	# combined fixup for "negative" w, a[]
	tstl	r5
	bgeq	1f
	addl2	(r3),r1
1:	tstl	(r3)
	bgeq	1f
	addl2	r5,r1

1:	movl	r0,(r2)+
	addl2	$4,r3
	movl	r1,r6

	sobgtr	r4,0b

	movl	r6,r0
	ret



#	.title	vax_bn_sqr_words  unsigned square, 32*32=>64
#;
#; w.j.m. 15-jan-1999
#;
#; it's magic ...
#;
#; void bn_sqr_words(ULONG r[],ULONG a[],int n) {
#;	int i;
#;	for(i = 0; i < n; i++) <r[2*i+1],r[2*i]> := a[i] * a[i] ;
#; }
#

ENTRY(bn_sqr_words,0)
	movl	4(ap),r2		# r
	movl	8(ap),r3		# a
	movl	12(ap),r4		# n

0:	movl	(r3)+,r5		# r5 = a[] & advance

	emul	r5,r5,$0,r0		# a[0] * a[0] + 0 -> r0

	# fixup for "negative" a[]
	tstl	r5
	bgeq	1f
	addl2	r5,r1
	addl2	r5,r1

1:	movq	r0,(r2)+		# store 64-bit result

	sobgtr	r4,0b			# loop

	ret


#	.title	vax_bn_div_words  unsigned divide
#;
#; Richard Levitte 20-Nov-2000
#;
#; ULONG bn_div_words(ULONG h, ULONG l, ULONG d)
#; {
#;	return ((ULONG)((((ULLONG)h)<<32)|l) / (ULLONG)d);
#; }
#;
#; Using EDIV would be very easy, if it didn't do signed calculations.
#; Any time any of the input numbers are signed, there are problems,
#; usually with integer overflow, at which point it returns useless
#; data (the quotient gets the value of l, and the remainder becomes 0).
#;
#; If it was just for the dividend, it would be very easy, just divide
#; it by 2 (unsigned), do the division, multiply the resulting quotient
#; and remainder by 2, add the bit that was dropped when dividing by 2
#; to the remainder, and do some adjustment so the remainder doesn't
#; end up larger than the divisor.  For some cases when the divisor is
#; negative (from EDIV's point of view, i.e. when the highest bit is set),
#; dividing the dividend by 2 isn't enough, and since some operations
#; might generate integer overflows even when the dividend is divided by
#; 4 (when the high part of the shifted down dividend ends up being exactly
#; half of the divisor, the result is the quotient 0x80000000, which is
#; negative...) it needs to be divided by 8.  Furthermore, the divisor needs
#; to be divided by 2 (unsigned) as well, to avoid more problems with the sign.
#; In this case, a little extra fiddling with the remainder is required.
#;
#; So, the simplest way to handle this is always to divide the dividend
#; by 8, and to divide the divisor by 2 if it's highest bit is set.
#; After EDIV has been used, the quotient gets multiplied by 8 if the
#; original divisor was positive, otherwise 4.  The remainder, oddly
#; enough, is *always* multiplied by 8.
#; NOTE: in the case mentioned above, where the high part of the shifted
#; down dividend ends up being exactly half the shifted down divisor, we
#; end up with a 33 bit quotient.  That's no problem however, it usually
#; means we have ended up with a too large remainder as well, and the
#; problem is fixed by the last part of the algorithm (next paragraph).
#;
#; The routine ends with comparing the resulting remainder with the
#; original divisor and if the remainder is larger, subtract the
#; original divisor from it, and increase the quotient by 1.  This is
#; done until the remainder is smaller than the divisor.
#;
#; The complete algorithm looks like this:
#;
#; d'    = d
#; l'    = l & 7
#; [h,l] = [h,l] >> 3
#; [q,r] = floor([h,l] / d)	# This is the EDIV operation
#; if (q < 0) q = -q		# I doubt this is necessary any more
#;
#; r'    = r >> 29
#; if (d' >= 0)
#;   q'  = q >> 29
#;   q   = q << 3
#; else
#;   q'  = q >> 30
#;   q   = q << 2
#; r     = (r << 3) + l'
#;
#; if (d' < 0)
#;   {
#;     [r',r] = [r',r] - q
#;     while ([r',r] < 0)
#;       {
#;         [r',r] = [r',r] + d
#;         [q',q] = [q',q] - 1
#;       }
#;   }
#;
#; while ([r',r] >= d')
#;   {
#;     [r',r] = [r',r] - d'
#;     [q',q] = [q',q] + 1
#;   }
#;
#; return q
#
#;r2 = l, q
#;r3 = h, r
#;r4 = d
#;r5 = l'
#;r6 = r'
#;r7 = d'
#;r8 = q'
#

ENTRY(bn_div_words,R6|R7|R8)
	movl	4(ap),r3		# h
	movl	8(ap),r2		# l
	movl	12(ap),r4		# d

	bicl3	$-8,r2,r5		# l' = l & 7
	bicl3	$7,r2,r2

	bicl3	$-8,r3,r6
	bicl3	$7,r3,r3

	addl2	r6,r2

	rotl	$-3,r2,r2		# l = l >> 3
	rotl	$-3,r3,r3		# h = h >> 3

	movl	r4,r7			# d' = d

	clrl	r6			# r' = 0
	clrl	r8			# q' = 0

	tstl	r4
	beql	0f			# Uh-oh, the divisor is 0...
	bgtr	1f
	rotl	$-1,r4,r4	# If d is negative, shift it right.
	bicl2	$0x80000000,r4	# Since d is then a large number, the
				# lowest bit is insignificant
				# (contradict that, and I'll fix the problem!)
1:
	ediv	r4,r2,r2,r3		# Do the actual division

	tstl	r2
	bgeq	1f
	mnegl	r2,r2		# if q < 0, negate it
1:
	tstl	r7
	blss	1f
	rotl	$3,r2,r2	#   q = q << 3
	bicl3	$-8,r2,r8	#   q' gets the high bits from q
	bicl3	$7,r2,r2
	brb	2f

1:				# else
	rotl	$2,r2,r2	#   q = q << 2
	bicl3	$-4,r2,r8	#   q' gets the high bits from q
	bicl3	$3,r2,r2
2:
	rotl	$3,r3,r3	# r = r << 3
	bicl3	$-8,r3,r6	# r' gets the high bits from r
	bicl3	$7,r3,r3
	addl2	r5,r3		# r = r + l'

	tstl	r7
	bgeq	5f
	bitl	$1,r7
	beql	5f		# if d' < 0 && d' & 1
	subl2	r2,r3		#   [r',r] = [r',r] - [q',q]
	sbwc	r8,r6
3:
	bgeq	5f		#   while r < 0
	decl	r2		#     [q',q] = [q',q] - 1
	sbwc	$0,r8
	addl2	r7,r3		#     [r',r] = [r',r] + d'
	adwc	$0,r6
	brb	3b

# The return points are placed in the middle to keep a short distance from
# all the branch points
1:
#	movl	r3,r1
	movl	r2,r0
	ret
0:
	movl	$-1,r0
	ret
5:
	tstl	r6
	bneq	6f
	cmpl	r3,r7
	blssu	1b		# while [r',r] >= d'
6:
	subl2	r7,r3		#   [r',r] = [r',r] - d'
	sbwc	$0,r6
	incl	r2		#   [q',q] = [q',q] + 1
	adwc	$0,r8
	brb	5b



#	.title	vax_bn_add_words  unsigned add of two arrays
#;
#; Richard Levitte 20-Nov-2000
#;
#; ULONG bn_add_words(ULONG r[], ULONG a[], ULONG b[], int n) {
#;	ULONG c = 0;
#;	int i;
#;	for (i = 0; i < n; i++) <c,r[i]> = a[i] + b[i] + c;
#;	return(c);
#; }
#

ENTRY(bn_add_words,0)
	movl	4(ap),r2	# r
	movl	8(ap),r3	# a
	movl	12(ap),r4	# b
	movl	16(ap),r5	# n
	clrl	r0

	tstl	r5
	bleq	1f

0:	movl	(r3)+,r1	# carry untouched
	adwc	(r4)+,r1	# carry used and touched
	movl	r1,(r2)+	# carry untouched
	sobgtr	r5,0b		# carry untouched

	adwc	$0,r0
1:	ret

#;
#; Richard Levitte 20-Nov-2000
#;
#; ULONG bn_sub_words(ULONG r[], ULONG a[], ULONG b[], int n) {
#;	ULONG c = 0;
#;	int i;
#;	for (i = 0; i < n; i++) <c,r[i]> = a[i] - b[i] - c;
#;	return(c);
#; }
#

ENTRY(bn_sub_words,R6)
	movl	4(ap),r2	# r
	movl	8(ap),r3	# a
	movl	12(ap),r4	# b
	movl	16(ap),r5	# n
	clrl	r0

	tstl	r5
	bleq	1f

0:	movl	(r3)+,r6	# carry untouched
	sbwc	(r4)+,r6	# carry used and touched
	movl	r6,(r2)+	# carry untouched
	sobgtr	r5,0b		# carry untouched

1:	adwc	$0,r0
	ret

#
#	Ragge 20-Sep-2003
#
#	Multiply a vector of 4/8 longword by another.
#	Uses two loops and 16/64 emuls.
#

ENTRY(bn_mul_comba4,R6|R7|R8|R9)
	movl	$4,r9		# 4*4
	brb	6f

ENTRY(bn_mul_comba8,R6|R7|R8|R9)
	movl	$8,r9		# 8*8

6:	movl	8(ap),r3	# a[]
	movl	12(ap),r7	# b[]
	brb	5f

ENTRY(bn_sqr_comba4,R6|R7|R8|R9)
	movl	$4,r9		# 4*4
	brb 0f

ENTRY(bn_sqr_comba8,R6|R7|R8|R9)
	movl	$8,r9		# 8*8

0:
	movl	8(ap),r3	# a[]
	movl	r3,r7		# a[]

5:	movl	4(ap),r5	# r[]
	movl	r9,r8

	clrq	(r5)		# clear destinatino, for add.
	clrq	8(r5)
	clrq	16(r5)		# these only needed for comba8
	clrq	24(r5)

2:	clrl	r4		# carry
	movl	r9,r6		# inner loop count
	movl	(r7)+,r2	# value to multiply with

1:	emul	r2,(r3),r4,r0
	tstl	r4
	bgeq	3f
	incl	r1
3:	tstl	r2
	bgeq	3f
	addl2	(r3),r1
3:	tstl	(r3)
	bgeq	3f
	addl2	r2,r1

3:	addl2	r0,(r5)+	# add to destination
	adwc	$0,r1		# remember carry
	movl	r1,r4		# add carry in next emul
	addl2	$4,r3
	sobgtr	r6,1b

	movl	r4,(r5)		# save highest add result

	ashl	$2,r9,r4
	subl2	r4,r3
	subl2	$4,r4
	subl2	r4,r5

	sobgtr	r8,2b

	ret
