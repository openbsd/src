	.ALTERNATE
! test of macro substitution around &s


foo	MACRO	a,b
	x&a&b
	ENDM

	foo 3 2 
	END
