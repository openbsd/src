! test of literal text operator
	.ALTERNATE
foop	MACRO	str1,str2
	SDATA	"str1"
	SDATA	str2
	ENDM


	
	foop	this< is a <string> with angle brackets>
	foop 	this< is a string with spaces>
	foop	this < is a string with a !>>


	END
