

	.MACRO 	foo a b c=a
	\a \b \c \d
	.ENDM

	foo 1 2
	foo 1 2 3 4
	foo 1
	foo 


	.END
