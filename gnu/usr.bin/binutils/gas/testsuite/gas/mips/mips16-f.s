        .set noreorder
        .text
        nop
l1:     nop
        .section "foo"
        .word   l1+3
