        .set noreorder
        .text
        nop
l1:     nop
1:      nop
        nop
        .section "foo"
        .word   l1
        .word   l1+8
        .word   1b
        .word   1b+3
	.word	g1
	.word	g1+8
