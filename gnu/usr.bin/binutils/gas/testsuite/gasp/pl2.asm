

	.ALTERNATE

	! ok
	!! also ok

foo	MACRO	
	! you can see me
	!! but not me
	! you can see me
	!! but not me
	but this "SHOULD !!BE OK"
	ENDM

	foo


define 	MACRO	val1,val2
	DB 	val1	! this comment will show up 
	DB	val2	!! this on won't
	ENDM

	define	0,1


	END
	
