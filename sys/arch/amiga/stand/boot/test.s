	.text
	.globl _start
_start:
	lea	pc@(bla),a0
	movq	#1,d0
	rts
	.globl	bla
