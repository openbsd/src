# j test
	
text_label:	
	j	r0
	j.d	r0
	j.jd	r0
	j.nd	r0

	j.f	[r1]
	j.d.f	[r1]
	j.jd.f	[r1]
	j.nd.f	[r1]

	j	text_label
	jal	text_label
	jra	text_label
	jeq	text_label
	jz	text_label
	jne	text_label
	jnz	text_label
	jpl	text_label
	jp	text_label
	jmi	text_label
	jn	text_label
	jcs	text_label
	jc	text_label
	jlo	text_label
	jcc	text_label
	jnc	text_label
	jhs	text_label
	jvs	text_label
	jv	text_label
	jvc	text_label
	jnv	text_label
	jgt	text_label
	jge	text_label
	jlt	text_label
	jle	text_label
	jhi	text_label
	jls	text_label
	jpnz	text_label

	j	external_text_label

	j	0
