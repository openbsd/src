.text
.align 0
	
	mrs	r8, cpsr
	mrseq	r9, cpsr_all
	mrs	r2, spsr

	msr	cpsr, r1
	msrne	cpsr_flg, #0xf0000000
	msr	spsr_flg, r8
	msr	spsr_all, r9

