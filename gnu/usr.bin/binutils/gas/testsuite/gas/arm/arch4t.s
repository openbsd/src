.text
.align 0
	
	bx	r0
	bxeq	r1

foo:	
	ldrh	r3, foo
	ldrsh	r4, [r5]
	ldrsb	r4, [r1, r3]
	ldrsh	r1, [r4, r4]!
	ldreqsb	r1, [r5, -r3]
	ldrneh	r2, [r6], r7
	ldrccsh r2, [r7], +r8
	ldrsb	r2, [r3, #255]
	ldrsh	r1, [r4, #-250]
	ldrsb	r1, [r5, #+240]

	strh	r2, bar
	strneh	r3, [r3]
bar:
