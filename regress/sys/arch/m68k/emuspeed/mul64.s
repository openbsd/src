/*	$OpenBSD: mul64.s,v 1.2 2001/01/29 02:05:54 niklas Exp $	*/

/*
 * stack:
 * + 8: count
 * + 4: retads
 * + 0: d2
 */

	.globl _mul64ureg
_mul64ureg:
	movl	d2,sp@-
	movl	sp@(8),d2
L1:
	mulul	d2,d1:d0
	subql	#1,d2
	jne	L1
	movl	sp@+,d2
	rts

	.globl _mul64sreg
_mul64sreg:
	movl	d2,sp@-
	movl	sp@(8),d2
L2:
	mulsl	d2,d1:d0
	subql	#1,d2
	jne	L2
	movl	sp@+,d2
	rts

	.globl _mul64umem
_mul64umem:
	movl	d2,sp@-
	movl	sp@(8),d2
L3:
	mulul	sp@(8),d1:d0
	subql	#1,d2
	jne	L3
	movl	sp@+,d2
	rts

	.globl _mul64smem
_mul64smem:
	movl	d2,sp@-
	movl	sp@(8),d2
L4:
	mulsl	sp@(8),d1:d0
	subql	#1,d2
	jne	L4
	movl	sp@+,d2
	rts
