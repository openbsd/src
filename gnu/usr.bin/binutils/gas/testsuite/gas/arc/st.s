# st/sr test
	
	st	r0,[r1]
	st	r5,[r6,1]
	st	r7,[r8,-1]
	st	r9,[r10,255]
	st	r11,[r12,-256]
	st	r19,[foo]
	st	r20,[foo+4]

	stb	r0,[0]
	stw	r0,[0]
	st.a	r0,[0]
	st.di	r0,[0]
	stb.a.di r0,[r0]

	sr	r0,[r1]
	sr	r2,[status]
	sr	r3,[0x12345678]
