# Test unaligned reloc generation
	
	.data
foo:
	.byte 0x1
	.uaword		fred
	.byte 0x2
	.uahalf		jim
	.byte 0x3
	.uaxword	harry
	.byte 0x4
	.byte 0x5
	.byte 0x6


