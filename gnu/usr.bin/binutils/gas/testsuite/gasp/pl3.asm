	.ALTERNATE

foo	MACRO	string
	LOCAL	lab1, lab2
lab1:	DATA.L	lab2
lab2:	SDATA	string
	ENDM

	foo	"An example"
	foo	"using LOCAL"

! test of LOCAL directive

chk_err	MACRO	limit
	LOCAL		skip !! frob
	LOCAL		zap,dog,barf
barf:	cmp		ax,limit	!! check value against
					!! limit
	jle		skip		!! skip call if OK
skip:	call	 	error
	foo		dog
	zap		dog	
	nop
	ENDM

	chk_err 5
	chk_err 10


	END
