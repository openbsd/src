fp	.req	r11
ip	.req	r12
sp	.req	r13
lr	.req	r14
pc	.req	r15

.text
	.align	0

	.global	mcount
mcount:
	stmfd	sp!, {lr}
	mov	r1, lr
	mov	r0, ip
	bl	__mcount
	ldmfd	sp!, {lr}
	add	pc, lr, #0x00000004
