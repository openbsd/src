.data
	.byte 0
.section A
	.byte 1
.pushsection B
	.byte 2
.pushsection C
	.byte 3
.popsection
	.byte 2
.popsection
	.byte 1
.previous
	.byte 0
.previous
	.byte 1
