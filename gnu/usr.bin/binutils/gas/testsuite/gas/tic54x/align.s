* alignment directives
* .even == .align 1, .even 2 == longword boundary
* .align [size]  ; size is number of words (value must be a power of 2)
	.global even, align2, align8, align128
	.field	2, 3
	.field	11, 8
	.word	0x1, 0x2
	.even	
even	.word	0x3
	.align	2
align2	.string	"abcde"
	.align	8
align8	.word	8
	.word	0,1,2,3,4,5,6,7
	.align
align128 .byte	4
	.word	0,1,2,3,4,5,6,7
	.data
	.field	2, 3
	.field	11, 8
	.word	0x1, 0x2
	.even
	.word	0x3
	.end	
