
	.ALTERNATE
	SDATA	%1+2+3 
	SDATA	"5"



	MACRO	foo
	SDATA	"HI"	!! this will go
	SDATA	"THERE	! this will stay
	ENDM

	foo


	SDATA	<!<this is <a wacky> example!>!!>
	SDATA	"<this is <a wacky> example>!"
	END
