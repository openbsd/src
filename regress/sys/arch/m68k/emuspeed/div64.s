/*
 * stack:
 * + 8: count
 * + 4: retads
 * + 0: d2
 */

	.globl _div64ureg
_div64ureg:
	movl	d2,sp@-
	movl	sp@(8),d2
L1:
	divul	d2,d1:d0
	subql	#1,d2
	jne	L1
	movl	sp@+,d2
	rts

	.globl _div64sreg
_div64sreg:
	movl	d2,sp@-
	movl	sp@(8),d2
L2:
	divsl	d2,d1:d0
	subql	#1,d2
	jne	L2
	movl	sp@+,d2
	rts

	.globl _div64umem
_div64umem:
	movl	d2,sp@-
	movl	sp@(8),d2
L3:
	divul	sp@(8),d1:d0
	subql	#1,d2
	jne	L3
	movl	sp@+,d2
	rts

	.globl _div64smem
_div64smem:
	movl	d2,sp@-
	movl	sp@(8),d2
L4:
	divsl	sp@(8),d1:d0
	subql	#1,d2
	jne	L4
	movl	sp@+,d2
	rts
