.text
	.type _start,@function
_start:

	break.x	0
	break.x	0x3fffffffffffffff
	
	nop.x	0
	nop.x	0x3fffffffffffffff

	movl r4 = 0
	movl r4 = 0xffffffffffffffff
	movl r4 = 0x1234567890abcdef

