        .section        .gcc_except_table,"aw",@progbits
        .section        .gnu.linkonce.t.blah,"ax",@progbits
.L0:
	jmp .L1
.L1:
        .section        .gcc_except_table,"aw",@progbits
        .uleb128 .L1-.L0
