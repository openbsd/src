	@ test ADRL pseudo-op
.text
foo:	
.align 0
1:
        .space 8192
2:
        adrl    r0, 1b
	adrl	r0, 1f
        adrl    r0, 2b
	adrl	r0, 2f
2:
	.space 8200
1:
