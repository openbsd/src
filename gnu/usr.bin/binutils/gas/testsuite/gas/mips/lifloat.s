# Source file used to test the li.d and li.s macros.
	
foo:	
	li.d	$4,1.0
	li.d	$f4,1.0
	
	li.s	$4,1.0
	li.s	$f4,1.0

# Round to a 16 byte boundary, for ease in testing multiple targets.
	.ifndef	EMPIC
	nop
	.endif
