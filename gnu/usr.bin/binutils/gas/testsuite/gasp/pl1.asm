	
	.ALTERNATE

alloc 	MACRO val1,val2
	DB	val1
	DB 	val2
	ENDM

	alloc	"that's" 'show biz'
	alloc	0,1
	alloc	0 1
	alloc	0	1
	alloc	,1


	




