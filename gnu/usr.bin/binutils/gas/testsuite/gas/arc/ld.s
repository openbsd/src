# ld/lr test
	
	ld	r0,[r1]
	ld	r2,[r3,r4]
	ld	r5,[r6,1]
	ld	r7,[r8,-1]
	ld	r9,[r10,255]
	ld	r11,[r12,-256]
	ld	r13,[r14,256]
	ld	r15,[r16,-257]
	ld	r17,[0x12345678,r28]
	ld	r19,[foo]
	ld	r20,[foo+4]

	ldb	r0,[0]
	ldw	r0,[0]
	ld.x	r0,[0]
	ld.a	r0,[0]
	ld.di	r0,[0]
	ldb.x.a.di r0,[r0]

	lr	r0,[r1]
	lr	r2,[status]
	lr	r3,[0x12345678]
