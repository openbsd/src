	.if	0
	.if	1
	.endc
	.long	0
	.if	0
	.long	1
	.endc
	.else
	.if	1
	.endc
	.long	2
	.if	0
	.long	3
	.else
	.long	4
	.endc
	.endc
	.p2align 5,0
