//	
// Test mutex relation handling	
//	
.text
	.explicit
start:	
	cmp.eq	p6, p0 = r29, r0
	add	r26 = r26, r29
	ld8	r29 = [r26]
