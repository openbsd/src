

	.SDATA	"HI","STEVE"
	.SDATA	"HI" , "STEVE" , <72>,<73>,<83><69><86><69>

	.SDATA	"H""I" , "STEVE" , <72>,<73>,<83><69><86><69>



	.SDATA	"SHOULD NOT FAIL" "HERE" 
	.SDATA	"SHOULD FAIL"  foo "HERE" 

	.SDATAB	8,"BOINK"

	; examples from book

	.SDATAB	2,"AAAAA"
	.SDATAB	2,"""BBB"""
	.SDATAB	2,"AABB"<H'07>


a1:	.SDATAZ	"HI"
a2:	.SDATAC "HI"
a3:	.SDATA	"HI"
