loop	macro	arg1,arg2,arg3
	dc.l	NARG
	ifne	NARG
	dc.l	arg1
	loop	arg2,arg3
	endc
	endm

	loop	1,2,3
