a1	.req	r0
a2	.req	r1
a3	.req	r2
a4	.req	r3
v1	.req	r4
v2	.req	r5
v3	.req	r6
v4	.req	r7
v5	.req	r8
v6	.req	r9
v7	.req	r10
fp	.req	r11
ip	.req	r12
sp	.req	r13
lr	.req	r14
pc	.req	r15

.text

	.global	__rt_sdiv
__rt_sdiv:
	b	x_divide

	.global	__rt_udiv
__rt_udiv:
	b	x_udivide

	.global	___umodsi3
___umodsi3:
	mov	a3, a1
	mov	a1, a2
	mov	a2, a3
	b	x_uremainder

	.global	___udivsi3
___udivsi3:
	mov	a3, a1
	mov	a1, a2
	mov	a2, a3
	b	x_udivide

	.global	___modsi3
___modsi3:
	mov	a3, a1
	mov	a1, a2
	mov	a2, a3
	b	x_remainder

	.global	___divsi3
___divsi3:
	mov	a3, a1
	mov	a1, a2
	mov	a2, a3
	b	x_divide

	.global	x_divtest
x_divtest:
	mov	pc,lr

	.global	x_remainder
x_remainder:
	stmfd	sp!,{lr}
	bl	x_divide
	mov	a1,a2
	ldmfd	sp!,{pc}

	.global	x_uremainder
x_uremainder:
	stmfd	sp!,{lr}
	bl	x_udivide
	mov	a1,a2
	ldmfd	sp!,{pc}

x_overflow:
	mvn	a1,#0
	mov	pc,lr

	.global	x_udivide		/* a1 = a2 / a1; a2 = a2 % a1 */
x_udivide:
	cmp	a1,#1
	bcc	x_overflow
	beq	x_divide_l0
	mov	ip,#0
	movs	a2,a2
	bpl	x_divide_l1
	orr	ip,ip,#0x20000000	/* ip bit 0x20000000 = -ve a2 */
	movs	a2,a2,lsr #1
	orrcs	ip,ip,#0x10000000	/* ip bit 0x10000000 = bit 0 of a2 */
	b	x_divide_l1

x_divide_l0:				/* a1 == 1 */
	mov	a1,a2
	mov	a2,#0
	mov	pc,lr

	.global	x_divide		/* a1 = a2 / a1; a2 = a2 % a1 */
x_divide:
	cmp	a1,#1
	bcc	x_overflow
	beq	x_divide_l0
	ands	ip,a1,#0x80000000
	rsbmi	a1,a1,#0
	ands	a3,a2,#0x80000000
	eor	ip,ip,a3
	rsbmi	a2,a2,#0
	orr	ip,a3,ip,lsr #1	/* ip bit 0x40000000 = -ve division */
				/* ip bit 0x80000000 = -ve remainder */

x_divide_l1:
	mov	a3,#1
	mov	a4,#0

	cmp	a2,a1
	bcc	x_divide_b0
	cmp	a2,a1,lsl #1
	bcc	x_divide_b1
	cmp	a2,a1,lsl #2
	bcc	x_divide_b2
	cmp	a2,a1,lsl #3
	bcc	x_divide_b3
	cmp	a2,a1,lsl #4
	bcc	x_divide_b4
	cmp	a2,a1,lsl #5
	bcc	x_divide_b5
	cmp	a2,a1,lsl #6
	bcc	x_divide_b6
	cmp	a2,a1,lsl #7
	bcc	x_divide_b7
	cmp	a2,a1,lsl #8
	bcc	x_divide_b8
	cmp	a2,a1,lsl #9
	bcc	x_divide_b9
	cmp	a2,a1,lsl #10
	bcc	x_divide_b10
	cmp	a2,a1,lsl #11
	bcc	x_divide_b11
	cmp	a2,a1,lsl #12
	bcc	x_divide_b12
	cmp	a2,a1,lsl #13
	bcc	x_divide_b13
	cmp	a2,a1,lsl #14
	bcc	x_divide_b14
	cmp	a2,a1,lsl #15
	bcc	x_divide_b15
	cmp	a2,a1,lsl #16
	bcc	x_divide_b16
	cmp	a2,a1,lsl #17
	bcc	x_divide_b17
	cmp	a2,a1,lsl #18
	bcc	x_divide_b18
	cmp	a2,a1,lsl #19
	bcc	x_divide_b19
	cmp	a2,a1,lsl #20
	bcc	x_divide_b20
	cmp	a2,a1,lsl #21
	bcc	x_divide_b21
	cmp	a2,a1,lsl #22
	bcc	x_divide_b22
	cmp	a2,a1,lsl #23
	bcc	x_divide_b23
	cmp	a2,a1,lsl #24
	bcc	x_divide_b24
	cmp	a2,a1,lsl #25
	bcc	x_divide_b25
	cmp	a2,a1,lsl #26
	bcc	x_divide_b26
	cmp	a2,a1,lsl #27
	bcc	x_divide_b27
	cmp	a2,a1,lsl #28
	bcc	x_divide_b28
	cmp	a2,a1,lsl #29
	bcc	x_divide_b29
	cmp	a2,a1,lsl #30
	bcc	x_divide_b30
	cmp	a2,a1,lsl #31
	subhs	a2,a2,a1,lsl #31
	addhs	a4,a4,a3,lsl #31
	cmp	a2,a1,lsl #30
	subhs	a2,a2,a1,lsl #30
	addhs	a4,a4,a3,lsl #30
x_divide_b30:
	cmp	a2,a1,lsl #29
	subhs	a2,a2,a1,lsl #29
	addhs	a4,a4,a3,lsl #29
x_divide_b29:
	cmp	a2,a1,lsl #28
	subhs	a2,a2,a1,lsl #28
	addhs	a4,a4,a3,lsl #28
x_divide_b28:
	cmp	a2,a1,lsl #27
	subhsS	a2,a2,a1,lsl #27
	addhs	a4,a4,a3,lsl #27
x_divide_b27:
	cmp	a2,a1,lsl #26
	subhs	a2,a2,a1,lsl #26
	addhs	a4,a4,a3,lsl #26
x_divide_b26:
	cmp	a2,a1,lsl #25
	subhs	a2,a2,a1,lsl #25
	addhs	a4,a4,a3,lsl #25
x_divide_b25:
	cmp	a2,a1,lsl #24
	subhs	a2,a2,a1,lsl #24
	addhs	a4,a4,a3,lsl #24
x_divide_b24:
	cmp	a2,a1,lsl #23
	subhs	a2,a2,a1,lsl #23
	addhs	a4,a4,a3,lsl #23
x_divide_b23:
	cmp	a2,a1,lsl #22
	subhs	a2,a2,a1,lsl #22
	addhs	a4,a4,a3,lsl #22
x_divide_b22:
	cmp	a2,a1,lsl #21
	subhs	a2,a2,a1,lsl #21
	addhs	a4,a4,a3,lsl #21
x_divide_b21:
	cmp	a2,a1,lsl #20
	subhs	a2,a2,a1,lsl #20
	addhs	a4,a4,a3,lsl #20
x_divide_b20:
	cmp	a2,a1,lsl #19
	subhs	a2,a2,a1,lsl #19
	addhs	a4,a4,a3,lsl #19
x_divide_b19:
	cmp	a2,a1,lsl #18
	subhs	a2,a2,a1,lsl #18
	addhs	a4,a4,a3,lsl #18
x_divide_b18:
	cmp	a2,a1,lsl #17
	subhs	a2,a2,a1,lsl #17
	addhs	a4,a4,a3,lsl #17
x_divide_b17:
	cmp	a2,a1,lsl #16
	subhs	a2,a2,a1,lsl #16
	addhs	a4,a4,a3,lsl #16
x_divide_b16:
	cmp	a2,a1,lsl #15
	subhs	a2,a2,a1,lsl #15
	addhs	a4,a4,a3,lsl #15
x_divide_b15:
	cmp	a2,a1,lsl #14
	subhs	a2,a2,a1,lsl #14
	addhs	a4,a4,a3,lsl #14
x_divide_b14:
	cmp	a2,a1,lsl #13
	subhs	a2,a2,a1,lsl #13
	addhs	a4,a4,a3,lsl #13
x_divide_b13:
	cmp	a2,a1,lsl #12
	subhs	a2,a2,a1,lsl #12
	addhs	a4,a4,a3,lsl #12
x_divide_b12:
	cmp	a2,a1,lsl #11
	subhs	a2,a2,a1,lsl #11
	addhs	a4,a4,a3,lsl #11
x_divide_b11:
	cmp	a2,a1,lsl #10
	subhs	a2,a2,a1,lsl #10
	addhs	a4,a4,a3,lsl #10
x_divide_b10:
	cmp	a2,a1,lsl #9
	subhs	a2,a2,a1,lsl #9
	addhs	a4,a4,a3,lsl #9
x_divide_b9:
	cmp	a2,a1,lsl #8
	subhs	a2,a2,a1,lsl #8
	addhs	a4,a4,a3,lsl #8
x_divide_b8:
	cmp	a2,a1,lsl #7
	subhs	a2,a2,a1,lsl #7
	addhs	a4,a4,a3,lsl #7
x_divide_b7:
	cmp	a2,a1,lsl #6
	subhs	a2,a2,a1,lsl #6
	addhs	a4,a4,a3,lsl #6
x_divide_b6:
	cmp	a2,a1,lsl #5
	subhs	a2,a2,a1,lsl #5
	addhs	a4,a4,a3,lsl #5
x_divide_b5:
	cmp	a2,a1,lsl #4
	subhs	a2,a2,a1,lsl #4
	addhs	a4,a4,a3,lsl #4
x_divide_b4:
	cmp	a2,a1,lsl #3
	subhs	a2,a2,a1,lsl #3
	addhs	a4,a4,a3,lsl #3
x_divide_b3:
	cmp	a2,a1,lsl #2
	subhs	a2,a2,a1,lsl #2
	addhs	a4,a4,a3,lsl #2
x_divide_b2:
	cmp	a2,a1,lsl #1
	subhs	a2,a2,a1,lsl #1
	addhs	a4,a4,a3,lsl #1
x_divide_b1:
	cmp	a2,a1
	subhs	a2,a2,a1
	addhs	a4,a4,a3
x_divide_b0:

	tst	ip,#0x20000000
	bne	x_udivide_l1
	mov	a1,a4
	cmp	ip,#0
	rsbmi	a2,a2,#0
	movs	ip,ip,lsl #1
	rsbmi	a1,a1,#0
	mov	pc,lr

x_udivide_l1:
	tst	ip,#0x10000000
	mov	a2,a2,lsl #1
	orrne	a2,a2,#1
	mov	a4,a4,lsl #1
	cmp	a2,a1
	subhs	a2,a2,a1
	addhs	a4,a4,a3
	mov	a1,a4
	mov	pc,lr

