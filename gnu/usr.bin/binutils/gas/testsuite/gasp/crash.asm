
	Stuff to try and crash it

foo:	.MACRO	
	HI
bar:	.MACRO	
	THERE
	bar
	.ENDM	


	.ENDM
	foo
	foo
	foo
	foo
	foo
	bar



	
