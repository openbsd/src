	.macro	m arg1 arg2
	\arg1
	.exitm
	\arg2
	.endm

	m	".long r1",.garbage
