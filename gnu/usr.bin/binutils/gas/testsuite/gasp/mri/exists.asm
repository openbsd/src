exists	macro	arg1,arg2
	ifne	==arg2
	move	arg1,arg2
	elsec
	push	arg1
	endc
	endm

	exists	foo,bar
	exists	foo
