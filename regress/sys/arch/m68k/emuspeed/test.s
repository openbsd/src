	.globl _mul32smem
_mul32smem:
	movl	d2,sp@-
	movl	sp@(8),d2
L1:
	mulsl	sp@(8),d1
	subql	#1,d2
	jne	L1
	movl	sp@+,d2
	rts

	.globl _mul32sreg
_mul32sreg:
	movl	d2,sp@-
	movl	sp@(8),d2
L2:
	mulsl	d0,d1
	subql	#1,d2
	jne	L2
	movl	sp@+,d2
	rts

	.globl _illegal
_illegal:
	illegal
	rts
